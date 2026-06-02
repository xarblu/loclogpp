#pragma once

#include <libgpsmm.h>

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

    // only allow construction via helpers
    Point() = default;

public:
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
     * Getters
     */
    std::int64_t timestamp() const { return m_timestamp; }
    float latitude() const { return m_latitude; }
    float longitude() const { return m_longitude; }
    std::optional<float> speed() const { return m_speed; }
    std::optional<float> accuracy() const { return m_accuracy; }
    std::optional<float> altitude() const { return m_altitude; }

    /**
     * Convert to formatted string
     */
    std::string toString() const;
};

} // namespace LocLogPP
