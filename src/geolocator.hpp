#pragma once

#include "point.hpp"

#include <libgpsmm.h>

#include <memory>
#include <string>
#include <optional>

namespace LocLogPP {

class Geolocator {
    std::unique_ptr<gpsmm> m_gps{nullptr};

    /**
     * host and port need to outlive gpsmm
     * because it'll keep a pointer to the contained
     * c_str
     */
    std::string m_host{};
    std::string m_port{};

    /**
     * Parameters
     */
    double m_requiredAccuracyMeters{1000.0};
    double m_requiredDistanceMeters{5.0};

    /**
     * Last point returned by awaitPoint()
     */
    std::optional<Point> m_lastPoint{std::nullopt};

    /**
     * Only allow creation with create()
     */
    explicit Geolocator(std::unique_ptr<gpsmm> &&gps, std::string &&host, std::string &&port)
        : m_gps{std::move(gps)}
        , m_host{std::move(host)}
        , m_port{std::move(port)}
    {}

    /**
     * Apply point filters
     * 
     * Returns nullopt if filtered, else the original point
     */
    std::optional<Point> filterPoint(Point point) const;

public:
    /**
     * Create a Geolocator
     * Connects to the given GPSD host+port (or default)
     * and enables streaming
     *
     * nullptr on error
     */
    static std::unique_ptr<Geolocator> create(std::string host = "localhost", std::string port = DEFAULT_GPSD_PORT);

    /**
     * No copying
     */
    Geolocator(Geolocator &other) = delete;
    void operator=(Geolocator &other) = delete;

    /**
     * Await the next point and get a pointer to it
     */
    std::optional<Point> awaitPoint();
};

} // namespace LocLogPP
