#include "geolocator.hpp"

#include "argparser.hpp"
#include "database.hpp"
#include "logger.hpp"
#include "point.hpp"

#include <gps.h>
#include <libgpsmm.h>

#include <memory>
#include <format>
#include <optional>
#include <functional>
#include <chrono>
#include <cmath>
#include <thread>

static inline std::string stateToString(LocLogPP::Geolocator::State state) {
    switch (state) {
        case LocLogPP::Geolocator::State::STATIONARY:
            return "STATIONARY";
        case LocLogPP::Geolocator::State::MOVING:
            return "MOVING";
    }
}

std::unique_ptr<LocLogPP::Geolocator> LocLogPP::Geolocator::create(std::shared_ptr<ArgParser> args, Database *db) {
    std::unique_ptr<Geolocator> geolocator{new Geolocator()};

    geolocator->m_args = args;
    geolocator->m_db = db;

    // Explicitly keep a copy of these attached to Geolocator.
    // GPSD stores a raw C string (aka char*) to them so they must outlive gpsmm
    geolocator->m_host = args->gpsdHost();
    geolocator->m_port = args->gpsdPort();

    Logger::info("Creating Geolocator with parameters:");
    Logger::info("  host: {}", geolocator->m_host);
    Logger::info("  port: {}", geolocator->m_port);
    Logger::info("  pointIntervalSeconds: {}", geolocator->m_args->pointIntervalSeconds());
    Logger::info("  requiredAccuracyMeters: {}", geolocator->m_args->requiredAccuracyMeters());
    Logger::info("  requiredDistanceMeters: {}", geolocator->m_args->requiredDistanceMeters());

    geolocator->m_gps = std::make_unique<gpsmm>(geolocator->m_host.c_str(), geolocator->m_port.c_str());
    if (!geolocator->m_gps->is_open()) {
        Logger::error("Could not connect to GPSD at {}:{}", geolocator->m_host, geolocator->m_port);
        return nullptr;
    }

    if (geolocator->m_gps->stream(WATCH_ENABLE|WATCH_JSON) == NULL) {
        Logger::error("Could not enable streaming on GPSD at {}:{}", geolocator->m_host, geolocator->m_port);
        return nullptr;
    }

    // seed inital last point from DB
    auto points = geolocator->m_db->getPoints();
    Logger::info("Database contains {} points", points.size());
    if (!points.empty()) {
        Logger::info("Last point:\n{}", points.back().toString());
        geolocator->m_lastPoint = points.back();
        geolocator->m_pastPoints.push_back(points.back());
    }

    return geolocator;
}

std::optional<LocLogPP::Point> LocLogPP::Geolocator::preFilterPoint(Point point) const {
    Logger::debug("Applying point pre-filter");

    // basic accuracy filter
    if (point.accuracy()) {
        auto accuracy = point.accuracy().value();
        if (accuracy > m_args->requiredAccuracyMeters()) {
            Logger::debug("Point accuracy insufficient ({:.3f} m)", accuracy);
            return std::nullopt;
        }
        Logger::debug("Point accuracy sufficient ({:.3f} m)", accuracy);
    }

    // filter excessive jumps
    if (!m_pastPoints.empty()) {
        const auto &lastPoint = m_pastPoints.back();

        const double distance = lastPoint.distance(point);

        // delta in seconds
        const double timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(point.timestamp() - lastPoint.timestamp()).count() / 1000000.0;

        double speed{0.0};
        if (timeDelta > 0) {
            speed = distance / timeDelta;
        }

        // we'll cap the speed at 1000 km/h
        // which is almost the speed of sound (1235 km/h)
        // and probably reasonably
        constexpr double limit{1000.0 / 3.6};
        if (speed > limit) {
            Logger::debug("Point jumped at unreasonable speed ({:.3f} m in {:.3f} s, avg. speed: {:.3f} m/s, limit: {:.3f} m/s)", distance, timeDelta, speed, limit);
            return std::nullopt;
        }
        Logger::debug("Point jumped within reasonable speed ({:.3f} m in {:.3f} s, avg. speed: {:.3f} m/s, limit: {:.3f} m/s)", distance, timeDelta, speed, limit);
    }

    return point;
}

std::optional<LocLogPP::Point> LocLogPP::Geolocator::filterPoint(Point point) const {
    Logger::debug("Applying point filter");

    if (m_lastPoint) {
        const auto distance = m_lastPoint->distance(point);
        const auto requiredDistance = m_args->requiredDistanceMeters();

        if (distance < requiredDistance) {
            Logger::debug("Distance to last point insufficient ({:.3f} m, required {:.3f} m)", distance, requiredDistance);
            return std::nullopt;
        }
        Logger::debug("Distance to last point sufficient ({:.3f} m, required {:.3f})", distance, requiredDistance);
    }

    return point;
}

std::optional<LocLogPP::Point> LocLogPP::Geolocator::pastPointsCenter() const {
    if (m_pastPoints.empty()) {
        return std::nullopt;
    }

    double latSum{0.0};
    double lonSum{0.0};

    double speedSum{0.0};
    int speedCount{0};

    double accSum{0.0};
    int accCount{0};

    double altSum{0.0};
    int altCount{0};

    for (const auto &point : m_pastPoints) {
        latSum += point.latitude();
        lonSum += point.longitude();

        if (const auto &speed = point.speed()) {
            speedSum += speed.value();
            speedCount += 1;
        }

        if (const auto &acc = point.accuracy()) {
            accSum += acc.value();
            accCount += 1;
        }

        if (const auto &alt = point.altitude()) {
            altSum += alt.value();
            altCount += 1;
        }
    }

    const double latMean{latSum / m_pastPoints.size()};
    const double lonMean{lonSum / m_pastPoints.size()};

    std::optional<double> speedMean{};
    if (speedCount > 0) {
        speedMean = speedSum / speedCount;
    }

    std::optional<double> accMean{};
    if (accCount > 0) {
        accMean = accSum / accCount;
    }

    std::optional<double> altMean{};
    if (altCount > 0) {
        altMean = altSum / altCount;
    }

    // XXX: does it make sense to use last points timestamp?
    return Point{m_pastPoints.back().timestamp(),
                 static_cast<float>(latMean),
                 static_cast<float>(lonMean),
                 speedMean,
                 accMean,
                 altMean};
}

void LocLogPP::Geolocator::evaluateState(LocLogPP::Point &point) {
    // We detect movement by calculating the center of the
    // older 75% and the newer 25% of recent points.
    // If these clusters are more than stationaryDistance
    // apart we enter MOVING state, else STATIONARY

    // threshold of cluster distance before going MOVING
    constexpr double stationaryDistance{15.0};

    // amount of points to keep for evaluation
    constexpr size_t evalPointsRequired{30};
    constexpr size_t evalPointsMax{60};

    // [0, pivot-1] belongs to old
    // [pivot, end] belongs to new
    const auto pivot{std::lround(static_cast<double>(m_pastPoints.size()) * 0.75)};

    // manage points
    m_pastPoints.push_back(point);
    if (m_pastPoints.size() < evalPointsRequired) {
        Logger::debug("Not enough points for mode evaluation (have {} need {})", m_pastPoints.size(), evalPointsRequired);
        return;
    }

    while (m_pastPoints.size() > evalPointsMax) {
        m_pastPoints.pop_front();
    }

    // older cluster
    double oldLat{0.0};
    double oldLon{0.0};
    int oldCount{0};

    for (auto it = m_pastPoints.begin(); it != m_pastPoints.begin() + pivot; it++) {
        oldLat += it->latitude();
        oldLon += it->longitude();
        oldCount += 1;
    }

    const double oldLatMean{oldLat / oldCount};
    const double oldLonMean{oldLon / oldCount};

    const Point oldCenter{std::chrono::system_clock::now(), static_cast<float>(oldLatMean), static_cast<float>(oldLonMean)};

    // newer cluster
    double newLat{0.0};
    double newLon{0.0};
    int newCount{0};

    for (auto it = m_pastPoints.begin() + pivot; it != m_pastPoints.end(); it++) {
        newLat += it->latitude();
        newLon += it->longitude();
        newCount += 1;
    }

    const double newLatMean{newLat / newCount};
    const double newLonMean{newLon / newCount};

    const Point newCenter{std::chrono::system_clock::now(), static_cast<float>(newLatMean), static_cast<float>(newLonMean)};

    const double distance = oldCenter.distance(newCenter);

    Logger::debug("Center of old cluster:\n{}", oldCenter.toString());
    Logger::debug("Center of new cluster:\n{}", newCenter.toString());
    Logger::debug("Distance: {:.3f} m", distance);

    // set state
    State state = (distance < stationaryDistance)
        ? State::STATIONARY 
        : State::MOVING;

    if (m_state != state) {
        Logger::info("State changed: {}", stateToString(state));
        m_state = state;
    }
}

int LocLogPP::Geolocator::trackInternal() {
    while (true) {
        // GPSD can send "the same point" multiple times (different NMEA sentences or something)
        // we'll merge those into a single point based on their timestamp
        // (that should be the same for all sentences)

        std::optional<Point> stagingPoint{std::nullopt};
        std::optional<Point> nextPoint{std::nullopt};

        while (true) {
            gps_data_t *data = nullptr;

            // this is not really a point interval,
            // just a timeout for "is data available yet?"
            if (!m_gps->waiting(5000000)) {
                continue;
            }

            data = m_gps->read();
            if (!data) {
                Logger::error("GPSD read error");
                return 1;
            }

            std::optional<Point> newPoint = Point::fromGPSD(*data);
            if (!newPoint) {
                Logger::debug("Ignoring GPSD read without valid point data");
                continue;
            }

            // this should only happen once during startup
            if (!stagingPoint) [[unlikely]] {
                stagingPoint.swap(newPoint);
                continue;
            }

            if (newPoint->timestamp() != stagingPoint->timestamp()) {
                nextPoint.swap(newPoint);
                break;
            }

            Logger::debug("Merging duplicate point");
            stagingPoint->update(newPoint.value());
        }

        // after this:
        // point -> stagingPoint
        // stagingPoint -> nextPoint
        // nextPoint -> nullopt
        std::optional<Point> point{std::nullopt};
        point.swap(stagingPoint);
        stagingPoint.swap(nextPoint);

        point = point.and_then(std::bind(&LocLogPP::Geolocator::preFilterPoint, this, std::placeholders::_1));
        if (!point) {
            Logger::debug("Ignoring point due to filters");
            continue;
        }

        evaluateState(point.value());

        // seconds since last returned point
        auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - m_lastPointTime).count();

        if (m_state == State::STATIONARY) {
            // this is our heartbeat interval while stationary
            if (elapsedSeconds < m_args->stationaryHeartbeatSeconds()) {
                continue;
            }

            Logger::info("Stationary heartbeat reached");

            // heartbeat ignores filters and intead uses
            // the center of past points
            // we just fed evaluateMode so this can't be nullopt
            point = pastPointsCenter().value();
        } else {
            // this is our regular interval while moving
            if (elapsedSeconds < m_args->pointIntervalSeconds()) {
                continue;
            }

            // actual data we care about
            point = point.and_then(std::bind(&LocLogPP::Geolocator::filterPoint, this, std::placeholders::_1));
            if (!point) {
                Logger::debug("Ignoring point due to filters");
                continue;
            }
        }

        Logger::info("Got point:\n{}", point->toString());
        m_lastPoint = point;
        m_lastPointTime = std::chrono::steady_clock::now();
        m_db->addPoint(point.value());
    }

    return 0;
}

int LocLogPP::Geolocator::track(std::shared_ptr<ArgParser> args, Database *db) {
    std::unique_ptr<Geolocator> geolocator{nullptr};
    while (!(geolocator = LocLogPP::Geolocator::create(args, db))) {
        Logger::error("Geolocator init failed");
        Logger::warn("Retrying in 5s");
        std::this_thread::sleep_for(std::chrono::seconds{5});
    }
    Logger::info("Geolocator initialized");

    while (true) {
        int ret = geolocator->trackInternal();
        if (ret > 0) {
            Logger::error("Internal tracker loop died");
            Logger::warn("Re-creating Geolocator");
            while (!(geolocator = LocLogPP::Geolocator::create(args, db))) {
                Logger::error("Geolocator init failed");
                Logger::warn("Retrying in 5s");
                std::this_thread::sleep_for(std::chrono::seconds{5});
            }
            continue;
        }
    }

    return 0;
}
