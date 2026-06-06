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

    double latSum{};
    double lonSum{};

    for (const auto &point : m_pastPoints) {
        latSum += point.latitude();
        lonSum += point.longitude();
    }

    const double latMean{latSum / m_pastPoints.size()};
    const double lonMean{lonSum / m_pastPoints.size()};

    return Point{m_pastPoints.back().timestamp(), static_cast<float>(latMean), static_cast<float>(lonMean)};
}

void LocLogPP::Geolocator::evaluateMode(LocLogPP::Point &point) {
    m_pastPoints.push_back(point);

    // past points used
    constexpr size_t evalPoints{10};

    if (m_pastPoints.size() < evalPoints) {
        Logger::debug("Not enough points for mode evaluation (have {} need {})", m_pastPoints.size(), evalPoints);
        return;
    }

    while (m_pastPoints.size() > evalPoints) {
        m_pastPoints.pop_front();
    }

    // If the past evalPoints never left
    // a certain radius around their center
    // we'll enter stationary mode

    // we ensured m_pastPoints isn't empty so this will
    // never be nullopt
    const Point center = pastPointsCenter().value();

    // if any point exceeds this we are moving
    constexpr double stationaryRadius{10.0};

    State state = State::STATIONARY;
    for (const auto &point : m_pastPoints) {
        if (point.distance(center) > stationaryRadius) {
            state = State::MOVING;
            break;
        }
    }

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

        evaluateMode(point.value());

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
