#include "geolocator.hpp"

#include "point.hpp"

#include <gps.h>
#include <libgpsmm.h>

#include <memory>
#include <iostream>
#include <format>
#include <optional>

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

std::optional<LocLogPP::Point> LocLogPP::Geolocator::awaitPoint() const {
    while (true) {
        gps_data_t *data = nullptr;

        // waiting time in us (1s)
        // TODO: configurable?
        if (!m_gps->waiting(1000000)) {
            continue;
        }

        if ((data = m_gps->read())) {
            if (auto point = Point::fromGPSD(*data)) {
                return point;
            } else {
                std::cerr << "Ignoring GPSD read without valid point data\n";
            }
        } else {
            std::cerr << "GPSD read error\n";
            return std::nullopt;
        }
    }
}
