#include "geolocator.hpp"

#include "point.hpp"
#include "logger.hpp"

#include <gps.h>
#include <libgpsmm.h>

#include <memory>
#include <format>
#include <optional>
#include <functional>
#include <chrono>

std::unique_ptr<LocLogPP::Geolocator> LocLogPP::Geolocator::create(std::string host, std::string port) {
    auto gps = std::make_unique<gpsmm>(host.c_str(), port.c_str());
    if (!gps->is_open()) {
        Logger::error("Could not connect to GPSD at {}:{}", host, port);
        return nullptr;
    }

    if (gps->stream(WATCH_ENABLE|WATCH_JSON) == NULL) {
        Logger::error("Could not enable streaming on GPSD at {}:{}", host, port);
        return nullptr;
    }

    return std::unique_ptr<Geolocator>(new Geolocator(std::move(gps), std::move(host), std::move(port)));
}

std::optional<LocLogPP::Point> LocLogPP::Geolocator::filterPoint(Point point) const {
    Logger::debug("Applying point filter");

    if (point.accuracy()) {
        auto accuracy = point.accuracy().value();
        if (accuracy > m_requiredAccuracyMeters) {
            Logger::debug("Point accuracy insufficient ({} m)", accuracy);
            return std::nullopt;
        }
        Logger::debug("Point accuracy sufficient ({} m)", accuracy);
    }

    if (m_lastPoint) {
        auto distance = m_lastPoint->distance(point);
        if (distance < m_requiredDistanceMeters) {
            Logger::debug("Distance to last point insufficient ({} m)", distance);
            return std::nullopt;
        }
        Logger::debug("Distance to last point sufficient ({} m)", distance);
    }

    return point;
}

std::optional<LocLogPP::Point> LocLogPP::Geolocator::awaitPoint() {
    while (true) {
        gps_data_t *data = nullptr;

        // this is not really a point interval,
        // just a timeout for "is data available yet?"
        if (!m_gps->waiting(5000000)) {
            continue;
        }

        // this is our interval, while not exeeded
        // just consume the GPSD data
        auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - m_lastPointTime).count();
        if (elapsedSeconds < m_pointIntervalSeconds) {
            data = m_gps->read();
            if (!data) {
                Logger::error("GPSD read error");
                return std::nullopt;
            }

            continue;
        }

        // actual data we care about
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
