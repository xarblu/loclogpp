#pragma once

#include <libgpsmm.h>
#include <sqlite3.h>

#include <cstdint>
#include <optional>
#include <string>

namespace LocLogPP {

class Point {
    std::int64_t m_timestamp;
    float m_latitude;
    float m_longitude;
    std::optional<float> m_speed{std::nullopt};
    std::optional<float> m_accuracy{std::nullopt};
    std::optional<float> m_altitude{std::nullopt};

    // only allow default construction via helpers
    Point() = default;

public:
    /**
     * Public constructor only with all valid params
     */
    explicit Point(std::int64_t timestamp, float latitude, float longitude, std::optional<float> speed = std::nullopt, std::optional<float> accuracy = std::nullopt, std::optional<float> altitude = std::nullopt)
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
    static std::optional<Point> fromGPSD(gps_data_t &point);

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
    std::int64_t timestamp() const { return m_timestamp; }
    float latitude() const { return m_latitude; }
    float longitude() const { return m_longitude; }
    std::optional<float> speed() const { return m_speed; }
    std::optional<float> accuracy() const { return m_accuracy; }
    std::optional<float> altitude() const { return m_altitude; }

    /**
     * Get the distance between this and another point
     * using the haversine formula
     */
    double distance(const Point &other) const;

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
