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
    float m_latitude;
    float m_longitude;
    float m_speed;
    std::optional<float> m_accuracy{std::nullopt};
    std::optional<float> m_altitude{std::nullopt};

    // track and its uncertainty in degrees relative to true north
    double m_track{0.0};
    std::optional<double> m_epd{0.0};

    // only used internally
    double m_xdop{0.0};
    double m_ydop{0.0};
    double m_epx{0.0};
    double m_epy{0.0};
    double m_eps{0.0};

    // tied to altitude
    double m_epv{0.0};
    double m_vdop{0.0};

    // only allow default construction via helpers
    Point() = default;

public:
    /**
     * Public constructor only with all valid params
     */
    explicit Point(std::chrono::system_clock::time_point timestamp, float latitude, float longitude, float speed, std::optional<float> accuracy = std::nullopt, std::optional<float> altitude = std::nullopt)
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
    float latitude() const { return m_latitude; }
    float longitude() const { return m_longitude; }
    float speed() const { return m_speed; }
    std::optional<float> accuracy() const { return m_accuracy; }
    std::optional<float> altitude() const { return m_altitude; }
    double xdop() const { return m_xdop; }
    double ydop() const { return m_ydop; }
    double epx() const { return m_epx; }
    double epy() const { return m_epy; }
    double eps() const { return m_eps; }
    double epv() const { return m_epv; }
    double vdop() const { return m_vdop; }

    // track and its uncertainty in degrees relative to true north
    double track() const { return m_track; }
    std::optional<double> epd() const { return m_epd; }

    /**
     * Setters
     */
    void setLatitude(float value) { m_latitude = value; }
    void setLongitude(float value) { m_longitude = value; }
    void setAltitude(float value) { m_altitude = value; }

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
