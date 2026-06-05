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
        auto requiredDistance = m_args->requiredDistanceMeters();

        // (stationary only) add penalty for point inaccuracy
        // to avoid excessive jumping in low accuracy scenarios
        if (m_state == State::STATIONARY) {
            const auto accuracyLast = m_lastPoint->accuracy();
            const auto accuracyCurr = point.accuracy();
            if (accuracyLast && accuracyCurr) {
                requiredDistance += accuracyLast.value() + accuracyCurr.value();
            } else if (accuracyLast) {
                requiredDistance += 2.0 * accuracyLast.value();
            } else if (accuracyCurr) {
                requiredDistance += 2.0 * accuracyCurr.value();
            }
        }

        const auto distance = m_lastPoint->distance(point);
        if (distance < requiredDistance) {
            Logger::debug("Distance to last point insufficient ({:.3f} m, required {:.3f} m)", distance, requiredDistance);
            return std::nullopt;
        }
        Logger::debug("Distance to last point sufficient ({:.3f} m, required {:.3f})", distance, requiredDistance);
    }

    return point;
}

void LocLogPP::Geolocator::evaluateMode(LocLogPP::Point &point) {
    m_pastPoints.push_back(point);

    constexpr size_t evalPoints{10};
    if (m_pastPoints.size() < evalPoints) {
        Logger::debug("Not enough eoints for mode evaluation (have {} need {})", m_pastPoints.size(), evalPoints);
        return;
    }

    while (m_pastPoints.size() > evalPoints) {
        m_pastPoints.pop_front();
    }

    // If the past evalPoints never left
    // a certain radius around their center
    // we'll enter stationary mode

    double latSum{};
    double lonSum{};

    for (const auto &point : m_pastPoints) {
        latSum += point.latitude();
        lonSum += point.longitude();
    }

    const double latMean{latSum / m_pastPoints.size()};
    const double lonMean{lonSum / m_pastPoints.size()};

    const Point center{0, static_cast<float>(latMean), static_cast<float>(lonMean)};

    // if any point exceeds this we are moving
    constexpr double stationaryRadius{10};

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

        // this is our interval, while not exeeded
        // just consume the GPSD data
        auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - m_lastPointTime).count();
        if (elapsedSeconds < m_args->pointIntervalSeconds()) {
            continue;
        }

        // actual data we care about
        std::optional<Point> filtered = point.and_then(std::bind(&LocLogPP::Geolocator::filterPoint, this, std::placeholders::_1));
        if (!filtered) {
            Logger::debug("Ignoring point due to filters");
            continue;
        }

        m_lastPoint = point;
        m_lastPointTime = std::chrono::steady_clock::now();
        return point;
    }
}
