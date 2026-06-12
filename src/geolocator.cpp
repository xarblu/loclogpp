#include "geolocator.hpp"

#include "argparser.hpp"
#include "database.hpp"
#include "logger.hpp"
#include "point.hpp"
#include "kalmanfilter.hpp"
#include "utils.hpp"

#include <gps.h>
#include <libgpsmm.h>

#include <memory>
#include <format>
#include <optional>
#include <functional>
#include <chrono>
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

        const double limit = m_args->maxSpeedMetersPerSecond();
        if (speed > limit) {
            Logger::debug("Point jumped at speed above maximum ({:.3f} m in {:.3f} s, avg. speed: {:.3f} m/s, limit: {:.3f} m/s)",
                          distance, timeDelta, speed, limit);
            return std::nullopt;
        }
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

void LocLogPP::Geolocator::applyKalmanFilters(Point &point) {
    // fixed values while STATIONARY, with minimal error
    double speedLat{0.0};
    double speedLon{0.0};
    double speedErrorLatLon{1e-5};

    double speedAlt{0.0};
    double speedErrorAlt{1e-5};

    // only while MOVING use the actual values
    if (m_stationaryDetection.state == State::MOVING) {
        // degrees
        std::tie(speedLat, speedLon) = speedToLatLonDegPerSecond(point.latitude(), point.longitude(), point.speed(), point.track());
        speedErrorLatLon = metToDegSquared(point.eps() * point.eps());

        // plain meters
        speedAlt = point.climb();
        speedErrorAlt = point.epc() * point.epc();
    }

    // latitude
    Measurement measurementLat{
        .timestamp = point.timestamp(),
        .position = point.latitude(),
        .positionError = metToDegSquared((point.ydop() * point.ydop()) * (1.0 + point.epy() * point.epy())),
        .speed = speedLat,
        .speedError = speedErrorLatLon,
    };
    if (!m_filters.lat) {
        m_filters.lat = std::make_unique<KalmanFilter>(measurementLat);
    } else {
        point.setLatitude(m_filters.lat->update(measurementLat));
    }

    // longitude
    Measurement measurementLon{
        .timestamp = point.timestamp(),
        .position = point.longitude(),
        .positionError = metToDegSquared((point.xdop() * point.xdop()) * (1.0 + point.epx() * point.epx())),
        .speed = speedLon,
        .speedError = speedErrorLatLon,
    };
    if (!m_filters.lon) {
        m_filters.lon = std::make_unique<KalmanFilter>(measurementLon);
    } else {
        point.setLongitude(m_filters.lon->update(measurementLon));
    }

    // altitude (optional)
    // Point ensures if altitude exists all related metrics
    // (vdop, epv, etc.) are valid as well
    if (point.altitude()) {
        Measurement measurementAlt{
            .timestamp = point.timestamp(),
            .position = *point.altitude(),
            .positionError = (point.vdop() * point.vdop()) * (1.0 + point.epv() * point.epv()),
            .speed = speedAlt,
            .speedError = speedErrorAlt,
        };
        if (!m_filters.alt) {
            // KalmanFilter defaults assume degrees, altitude values are plain meters
            m_filters.alt = std::make_unique<KalmanFilter>(measurementAlt, 0.1, 3);
        } else {
            point.setAltitude(m_filters.alt->update(measurementAlt));
        }
    }
}

std::optional<LocLogPP::Point> LocLogPP::Geolocator::pastPointsCenter() const {
    if (m_pastPoints.empty()) {
        return std::nullopt;
    }

    double latSum{0.0};
    double lonSum{0.0};
    double speedSum{0.0};

    double accSum{0.0};
    int accCount{0};

    double altSum{0.0};
    int altCount{0};

    for (const auto &point : m_pastPoints) {
        latSum += point.latitude();
        lonSum += point.longitude();
        speedSum += point.speed();

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
    double speedMean{speedSum / m_pastPoints.size()};

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
                 latMean,
                 lonMean,
                 speedMean,
                 accMean,
                 altMean};
}

void LocLogPP::Geolocator::updateStationaryDetection(const Point &point) {
    auto &anchorPoint = m_stationaryDetection.anchorPoint;
    auto &stopCount = m_stationaryDetection.stopCount;
    auto &state = m_stationaryDetection.state;
    const auto &stopSpeedThreshold = m_stationaryDetection.stopSpeedThreshold;
    const auto &stopsRequired = m_stationaryDetection.stopsRequired;
    const auto &containmentRadius = m_stationaryDetection.containmentRadius;

    // MOVING but stopped according to speed
    if (!anchorPoint && point.speed() < stopSpeedThreshold) {
        // not STATIONARY yet
        if (stopCount < stopsRequired) {
            stopCount++;
            return;
        }

        // first time STATIONARY - lock to the last point
        anchorPoint = point;
        state = State::STATIONARY;
        Logger::info("State changed: {}", stateToString(state));
        return;
    }

    // MOVING and no signs of stopping
    if (!anchorPoint) {
        if (stopCount > 0) {
            stopCount--;
        }
        return;
    }

    // once locked anything within our containmentRadius is STATIONARY
    if (anchorPoint->distance(point) < containmentRadius) {
        // recover in case of a few bogus points
        // outside the containmentRadius
        if (stopCount < stopsRequired) {
            stopCount++;
        }

        return;
    }

    // stopped outside containmentRadius
    // keep state frozen until we either
    //  - go back into containmentRadius (STATIONARY)
    //  - continue moving outside containmentRadius (MOVING)
    if (point.speed() < stopSpeedThreshold) {
        return;
    }

    // started moving outside containmentRadius
    // but not MOVING yet
    if (stopCount > 0) {
        stopCount--;
        return;
    }

    // actually MOVING now
    anchorPoint.reset();
    state = State::MOVING;
    Logger::info("State changed: {}", stateToString(state));
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

            std::optional<Point> newPoint = Point::fromGPSD(*data, m_args.get());
            if (!newPoint) {
                continue;
            }
            Logger::debug("Received new point:\n{}", newPoint->toString());

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

        // after our basic "bad points" filter
        // throw them to Kalman for smoothing
        applyKalmanFilters(*point);

        Logger::debug("Point after filters:\n{}", point->toString());

        updateStationaryDetection(*point);

        // seconds since last returned point
        auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - m_lastPointTime).count();

        if (m_stationaryDetection.state == State::STATIONARY) {
            // this is our heartbeat interval while stationary
            if (elapsedSeconds < m_args->stationaryHeartbeatSeconds()) {
                continue;
            }

            Logger::info("Stationary heartbeat reached");

            // heartbeat uses the anchored point if it is available
            // (should always be the case when stationary but as a fallback
            // just keep the real point and warn)
            if (m_stationaryDetection.anchorPoint) [[likely]] {
                point = m_stationaryDetection.anchorPoint;
            } else {
                Logger::warn("STATIONARY without anchor point");
            }

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
        if (ret == 0) {
            Logger::info("Internal tracker loop shut down cleanly");
            break;
        }

        Logger::error("Internal tracker loop died");
        Logger::warn("Re-creating Geolocator");
        while (!(geolocator = LocLogPP::Geolocator::create(args, db))) {
            Logger::error("Geolocator init failed");
            Logger::warn("Retrying in 5s");
            std::this_thread::sleep_for(std::chrono::seconds{5});
        }
    }

    return 0;
}
