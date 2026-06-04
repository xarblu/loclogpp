#pragma once

#include "argparser.hpp"
#include "point.hpp"

#include <libgpsmm.h>

#include <memory>
#include <string>
#include <optional>
#include <chrono>

namespace LocLogPP {

class Geolocator {
    std::unique_ptr<gpsmm> m_gps{nullptr};

    /**
     * Parameters
     */
    std::shared_ptr<ArgParser> m_args;
    std::string m_host{};
    std::string m_port{};

    /**
     * Last point returned by awaitPoint()
     */
    std::optional<Point> m_lastPoint{std::nullopt};
    std::chrono::time_point<std::chrono::steady_clock> m_lastPointTime{};

    /**
     * Only allow creation with create()
     */
    Geolocator() = default;

    /**
     * Apply point filters
     * 
     * Returns nullopt if filtered, else the original point
     */
    std::optional<Point> filterPoint(Point point) const;

public:
    /**
     * Create a Geolocator
     * Connects to the given GPSD host+port
     * and enables streaming
     *
     * nullptr on error
     */
    static std::unique_ptr<Geolocator> create(std::shared_ptr<ArgParser> args);

    /**
     * No copying
     */
    Geolocator(Geolocator &other) = delete;
    void operator=(Geolocator &other) = delete;

    /**
     * Await the next point
     *
     * Returns nullopt on GPSD read error
     */
    std::optional<Point> awaitPoint();
};

} // namespace LocLogPP
