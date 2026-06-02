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

    explicit Geolocator(std::unique_ptr<gpsmm> &&gps, std::string &&host, std::string &&port)
        : m_gps{std::move(gps)}
        , m_host{std::move(host)}
        , m_port{std::move(port)}
    {}

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
    std::optional<Point> awaitPoint() const;
};

} // namespace LocLogPP
