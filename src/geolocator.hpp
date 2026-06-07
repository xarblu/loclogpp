#pragma once

#include "argparser.hpp"
#include "point.hpp"

#include <libgpsmm.h>

#include <memory>
#include <string>
#include <optional>
#include <chrono>
#include <deque>

namespace LocLogPP {

class Database;

class Geolocator {
public:
    enum class State {
        STATIONARY,
        MOVING,
    };

private:
    std::unique_ptr<gpsmm> m_gps{nullptr};
    Database *m_db{nullptr};

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
     * Past received points used to determine stationary/moving state
     */
    std::deque<Point> m_pastPoints{};

    /**
     * Current detected state
     */
    State m_state{State::STATIONARY};

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

    /**
     * Get center of m_pastPoints, the Point timestamp
     * will be copied from the last added point
     *
     * Returns nullopt if m_pastPoints does not contain any points
     */
    std::optional<Point> pastPointsCenter() const;

    /**
     * Store copy of the point and evaluate state
     */
    void evaluateState(Point &point);

public:
    /**
     * Create a Geolocator
     * Connects to the given GPSD host+port
     * and enables streaming
     *
     * nullptr on error
     */
    static std::unique_ptr<Geolocator> create(std::shared_ptr<ArgParser> args, Database *db);

    /**
     * Start main Geolocator tracking loop
     */
    static int track(std::shared_ptr<ArgParser> args, Database *db);

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
