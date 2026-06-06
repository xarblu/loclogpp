#include "geolocator.hpp"

#include "argparser.hpp"
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

static inline std::string stateToString(LocLogPP::Geolocator::State state) {
    switch (state) {
        case LocLogPP::Geolocator::State::STATIONARY:
            return "STATIONARY";
        case LocLogPP::Geolocator::State::MOVING:
            return "MOVING";
    }
}

std::unique_ptr<LocLogPP::Geolocator> LocLogPP::Geolocator::create(std::shared_ptr<ArgParser> args) {
    std::unique_ptr<Geolocator> geolocator{new Geolocator()};

    geolocator->m_args = args;

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

    return geolocator;
}

std::optional<LocLogPP::Point> LocLogPP::Geolocator::filterPoint(Point point) const {
    Logger::debug("Applying point filter");

    if (point.accuracy()) {
        auto accuracy = point.accuracy().value();
        if (accuracy > m_args->requiredAccuracyMeters()) {
            Logger::debug("Point accuracy insufficient ({:.3f} m)", accuracy);
            return std::nullopt;
        }
        Logger::debug("Point accuracy sufficient ({:.3f} m)", accuracy);
    }

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
    // older 50% and the newer 50% of recent points.
    // If these clusters are more than stationaryDistance
    // apart we enter MOVING state, else STATIONARY

    // threshold of cluster distance before going MOVING
    constexpr double stationaryDistance{10.0};

    // amount of points to keep for evaluation
    constexpr size_t evalPointsRequired{10};
    constexpr size_t evalPointsMax{50};

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
    double oldHalfLat{0.0};
    double oldHalfLon{0.0};
    int oldCount{0};

    for (auto it = m_pastPoints.begin(); it != m_pastPoints.begin() + m_pastPoints.size() / 2; it++) {
        oldHalfLat += it->latitude();
        oldHalfLon += it->longitude();
        oldCount += 1;
    }

    const double oldHalfLatMean{oldHalfLat / oldCount};
    const double oldHalfLonMean{oldHalfLon / oldCount};

    const Point oldHalfCenter{0, static_cast<float>(oldHalfLat), static_cast<float>(oldHalfLon)};

    // newer cluster
    double newHalfLat{0.0};
    double newHalfLon{0.0};
    int newCount{0};

    for (auto it = m_pastPoints.begin() + m_pastPoints.size() / 2; it != m_pastPoints.end(); it++) {
        newHalfLat += it->latitude();
        newHalfLon += it->longitude();
        newCount += 1;
    }

    const double newHalfLatMean{newHalfLat / newCount};
    const double newHalfLonMean{newHalfLon / newCount};

    const Point newHalfCenter{0, static_cast<float>(newHalfLat), static_cast<float>(newHalfLon)};

    const double distance = oldHalfCenter.distance(newHalfCenter);

    Logger::debug("Center of old cluster: {}", oldHalfCenter.toString());
    Logger::debug("Center of new cluster: {}", newHalfCenter.toString());
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

std::optional<LocLogPP::Point> LocLogPP::Geolocator::awaitPoint() {
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
            return std::nullopt;
        }

        std::optional<Point> point = Point::fromGPSD(*data);
        if (!point) {
            Logger::debug("Ignoring GPSD read without valid point data");
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
            std::optional<Point> filtered = point.and_then(std::bind(&LocLogPP::Geolocator::filterPoint, this, std::placeholders::_1));
            if (!filtered) {
                Logger::debug("Ignoring point due to filters");
                continue;
            }

        }

        m_lastPoint = point;
        m_lastPointTime = std::chrono::steady_clock::now();
        return point;
    }
}
