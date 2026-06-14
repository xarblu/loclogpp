#pragma once

#include <libgpsmm.h>
#include <sqlite3.h>

#include <optional>
#include <string>
#include <chrono>

namespace LocLogPP {

class ArgParser;

class Point {
    // exported values
    std::chrono::system_clock::time_point m_timestamp{};
    double m_latitude;
    double m_longitude;
    double m_speed;
    std::optional<double> m_accuracy{std::nullopt};

    // track and its uncertainty in degrees relative to true north
    double m_track{0.0};
    std::optional<double> m_epd{0.0};

    // only used internally
    double m_xdop{0.0};
    double m_ydop{0.0};
    double m_epx{0.0};
    double m_epy{0.0};
    double m_eps{0.0};

    // altitude values
    // if altitude has a value the other values
    // are ensured to be valid as well
    // otherwise they're garbage
    std::optional<double> m_altitude{std::nullopt};
    double m_epv{0.0};
    double m_vdop{0.0};
    double m_climb{0.0};
    double m_epc{0.0};

    // only allow default construction via helpers
    Point() = default;

public:
    /**
     * Public constructor only with all valid params
     */
    explicit Point(std::chrono::system_clock::time_point timestamp, double latitude, double longitude, double speed, std::optional<double> accuracy = std::nullopt, std::optional<double> altitude = std::nullopt)
        : m_timestamp{timestamp}
        , m_latitude{latitude}
        , m_longitude{longitude}
        , m_speed{speed}
        , m_accuracy{accuracy}
        , m_altitude{altitude}
    {}

    /**
     * Parse from GPSD point
     * Required values:
     *  fix.time
     *  fix.latitude
     *  fix.longitude
     *
     * Returns nullopt if not all required values were available
     */
    static std::optional<Point> fromGPSD(gps_data_t &point, ArgParser *args);

    /**
     * Parse from a row of our SQL points table
     * It is expected that the the statement was just advanced using sqlite3_step
     * returning SQLITE_ROW
     *
     * Returns nullopt on error
     */
    static std::optional<Point> fromSQL(sqlite3_stmt *statement);

    /**
     * Getters
     */
    std::chrono::system_clock::time_point timestamp() const { return m_timestamp; }
    double latitude() const { return m_latitude; }
    double longitude() const { return m_longitude; }
    double speed() const { return m_speed; }
    std::optional<double> accuracy() const { return m_accuracy; }
    double xdop() const { return m_xdop; }
    double ydop() const { return m_ydop; }
    double epx() const { return m_epx; }
    double epy() const { return m_epy; }
    double eps() const { return m_eps; }

    // altitude and related values
    std::optional<double> altitude() const { return m_altitude; }
    double epv() const { return m_epv; }
    double vdop() const { return m_vdop; }
    double climb() const { return m_climb; }
    double epc() const { return m_epc; }

    // track and its uncertainty in degrees relative to true north
    double track() const { return m_track; }
    std::optional<double> epd() const { return m_epd; }

    /**
     * Setters
     */
    void setLatitude(double value) { m_latitude = value; }
    void setLongitude(double value) { m_longitude = value; }
    void setAltitude(double value) { m_altitude = value; }
    void setSpeed(double value) { m_speed = value; }

    /**
     * Get the distance between this and another point
     * using the haversine formula
     */
    double distance(const Point &other) const;

    /**
     * Update info inside point with that of other
     */
    void update(const Point &other);

    /**
     * Convert to formatted string
     */
    std::string toString() const;

    /**
     * Convert to a SQL tuple (including parenthesis)
     *
     * The first entry is always NULL for autoincrementing ID
     */
    std::string toSQL() const;

    /**
     * Convert to GPX trkpt
     */
    std::string toGPX() const;
};

} // namespace LocLogPP
