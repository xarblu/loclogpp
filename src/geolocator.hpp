#pragma once

#include "argparser.hpp"
#include "kalmanfilter.hpp"
#include "point.hpp"

#include <libgpsmm.h>

#include <memory>
#include <string>
#include <optional>
#include <chrono>
#include <deque>

namespace LocLogPP {

class Database;
class KalmanFilter;

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
     * Kalman filters for lat, lon, alt
     *
     * lazily initialized and seeded by the received point
     */
    struct {
        std::unique_ptr<KalmanFilter> lat{nullptr};
        std::unique_ptr<KalmanFilter> lon{nullptr};
        std::unique_ptr<KalmanFilter> alt{nullptr};
    } m_filters;

    /**
     * Only allow creation with create()
     */
    Geolocator() = default;

    /**
     * Create a Geolocator
     * Connects to the given GPSD host+port
     * and enables streaming
     *
     * nullptr on error
     */
    static std::unique_ptr<Geolocator> create(std::shared_ptr<ArgParser> args, Database *db);

    /**
     * Apply point filters
     * preFilterPoint applies to all points right after parsing
     * filterPoints applies to the MOVING path only
     * 
     * Returns nullopt if filtered, else the original point
     */
    std::optional<Point> preFilterPoint(Point point) const;
    std::optional<Point> filterPoint(Point point) const;

    /**
     * Apply the Kalman filters for lat, lon, alt on the given Point
     */
    void applyKalmanFilters(Point &point);

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

    /**
     * Internal tracker loop
     *
     * Returns int > 0 on error
     */
    int trackInternal();

public:
    /**
     * Start main Geolocator tracking loop
     */
    static int track(std::shared_ptr<ArgParser> args, Database *db);

    /**
     * No copying
     */
    Geolocator(Geolocator &other) = delete;
    void operator=(Geolocator &other) = delete;
};

} // namespace LocLogPP
