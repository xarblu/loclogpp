#include "geolocator.hpp"

#include "point.hpp"

#include <gps.h>
#include <libgpsmm.h>

#include <memory>
#include <iostream>
#include <format>
#include <optional>
#include <functional>
#include <chrono>

std::unique_ptr<LocLogPP::Geolocator> LocLogPP::Geolocator::create(std::string host, std::string port) {
    auto gps = std::make_unique<gpsmm>(host.c_str(), port.c_str());
    if (!gps->is_open()) {
        std::cerr << std::format("Could not connect to GPSD at {}:{}\n", host, port);
        return nullptr;
    }

    if (gps->stream(WATCH_ENABLE|WATCH_JSON) == NULL) {
        std::cerr << std::format("Could not enable streaming on GPSD at {}:{}\n", host, port);
        return nullptr;
    }

    return std::unique_ptr<Geolocator>(new Geolocator(std::move(gps), std::move(host), std::move(port)));
}

std::optional<LocLogPP::Point> LocLogPP::Geolocator::filterPoint(Point point) const {
    std::cerr << "Applying point filter\n";

    if (point.accuracy()) {
        auto accuracy = point.accuracy().value();
        if (accuracy > m_requiredAccuracyMeters) {
            std::cerr << std::format("Point accuracy insufficient ({} m)\n", accuracy);
            return std::nullopt;
        }
        std::cerr << std::format("Point accuracy sufficient ({} m)\n", accuracy);
    }

    if (m_lastPoint) {
        auto distance = m_lastPoint->distance(point);
        if (distance < m_requiredDistanceMeters) {
            std::cerr << std::format("Distance to last point insufficient ({} m)\n", distance);
            return std::nullopt;
        }
        std::cerr << std::format("Distance to last point sufficient ({} m)\n", distance);
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
                std::cerr << "GPSD read error\n";
                return std::nullopt;
            }

            continue;
        }

        // actual data we care about
        data = m_gps->read();
        if (!data) {
            std::cerr << "GPSD read error\n";
            return std::nullopt;
        }

        auto point = Point::fromGPSD(*data);
        if (!point) {
            std::cerr << "Ignoring GPSD read without valid point data\n";
            continue;
        }

        auto filtered = point.transform(std::bind(&LocLogPP::Geolocator::filterPoint, this, std::placeholders::_1));
        if (!filtered) {
            std::cerr << "Ignoring point due to filters\n";
            continue;
        }

        m_lastPoint = point;
        m_lastPointTime = std::chrono::steady_clock::now();
        return point;
    }
}
