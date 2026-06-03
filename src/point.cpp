#include "point.hpp"

#include "logger.hpp"

#include <gps.h>
#include <libgpsmm.h>
#include <sqlite3.h>

#include <string>
#include <format>
#include <cmath>

std::optional<LocLogPP::Point> LocLogPP::Point::fromGPSD(gps_data_t &data) {
    if (!(data.set & MODE_SET)) {
        Logger::warn("fix.mode is required");
        return std::nullopt;
    }

    std::string fixModeStr;
    switch (data.fix.mode) {
        case MODE_NOT_SEEN:
            fixModeStr = "MODE_NOT_SEEN";
            break;
        case MODE_NO_FIX:
            fixModeStr = "MODE_NO_FIX";
            break;
        case MODE_2D:
            fixModeStr = "MODE_2D";
            break;
        case MODE_3D:
            fixModeStr = "MODE_3D";
            break;
        default:
            fixModeStr = "UNKNOWN";
            break;
    }
    Logger::info("fix.mode is {}", fixModeStr);

    if (data.fix.mode < MODE_2D) {
        Logger::warn("fix.mode must be at least MODE_2D");
        return std::nullopt;
    }

    Point point{};

    if (data.set & TIME_SET) {
        point.m_timestamp = data.fix.time.tv_sec;
    } else {
        Logger::warn("fix.time is required");
        return std::nullopt;
    }

    if ((data.set & LATLON_SET) && std::isfinite(data.fix.latitude) && std::isfinite(data.fix.longitude)) {
        point.m_latitude = data.fix.latitude;
        point.m_longitude = data.fix.longitude;
    } else {
        Logger::warn("fix.latitude and fix.longitude are required");
        return std::nullopt;
    }

    if ((data.set & ALTITUDE_SET) && std::isfinite(data.fix.altMSL)) {
        // altidude above mean sea level
        // this is what Owntracks uses as well
        // https://owntracks.org/booklet/tech/json/#_typelocation
        point.m_altitude = data.fix.altMSL;
    }

    if ((data.set & SPEED_SET) && std::isfinite(data.fix.speed)) {
        point.m_speed = data.fix.speed;
    }

    if ((data.set & HERR_SET) && std::isfinite(data.fix.eph)) {
        // only horizontal accuracy for now
        point.m_accuracy = data.fix.eph;
    }

    return point;
}

std::optional<LocLogPP::Point> LocLogPP::Point::fromSQL(sqlite3_stmt *statement) {
    if (!statement) [[unlikely]] {
        Logger::warn("Point::fromSQL() received nullptr");
        return std::nullopt;
    }

    // id INTEGER PRIMARY KEY,
    // timestamp INTEGER,
    // latitude REAL,
    // longitude REAL,
    // speed REAL,
    // accuracy REAL,
    // altitude REAL

    Point point{};

    point.m_timestamp = sqlite3_column_int64(statement, 1);
    point.m_latitude = sqlite3_column_double(statement, 2);
    point.m_longitude = sqlite3_column_double(statement, 3);
    if (sqlite3_column_type(statement, 4) != SQLITE_NULL) {
        point.m_speed = sqlite3_column_double(statement, 4);
    }
    if (sqlite3_column_type(statement, 5) != SQLITE_NULL) {
        point.m_accuracy = sqlite3_column_double(statement, 5);
    }
    if (sqlite3_column_type(statement, 6) != SQLITE_NULL) {
        point.m_altitude = sqlite3_column_double(statement, 6);
    }

    return point;
}

double LocLogPP::Point::distance(const Point &other) const {
    constexpr double EARTH_RADIUS_METERS{6371000.0};

    double latA = m_latitude * M_PI / 180.0;
    double latB = other.latitude() * M_PI / 180.0;

    double deltaLat = (m_latitude - other.latitude()) * M_PI / 180.0;
    double deltaLon = (m_longitude - other.longitude()) * M_PI / 180.0;

    double haversineLat = std::sin(deltaLat) * std::sin(deltaLat);
    double haversineLon = std::sin(deltaLon) * std::sin(deltaLon);

    double haversine = haversineLat + std::cos(latA) * std::cos(latB) * haversineLon;
    
    double distance = 2.0 * EARTH_RADIUS_METERS * std::atan2(std::sqrt(haversine), std::sqrt(1.0 - haversine));

    return distance;
}

std::string LocLogPP::Point::toString() const {
    return std::format(
        "timestamp: {}\n"
        "latitude: {}\n" 
        "longitude: {}\n"
        "altitude: {}\n" 
        "speed: {}\n"
        "accuracy: {}",
        m_timestamp,
        m_latitude,
        m_longitude,
        m_altitude.value_or(NAN),
        m_speed.value_or(NAN),
        m_accuracy.value_or(NAN));
}

std::string LocLogPP::Point::toSQL() const {
    std::string sqlPoint{"(NULL,"};
    sqlPoint += std::format(" {},", m_timestamp);
    sqlPoint += std::format(" {},", m_latitude);
    sqlPoint += std::format(" {},", m_longitude);
    if (m_speed) {
        sqlPoint += std::format(" {},", m_speed.value());
    } else {
        sqlPoint += std::format(" NULL,");
    }
    if (m_accuracy) {
        sqlPoint += std::format(" {},", m_accuracy.value());
    } else {
        sqlPoint += std::format(" NULL,");
    }
    if (m_altitude) {
        sqlPoint += std::format(" {},", m_altitude.value());
    } else {
        sqlPoint += std::format(" NULL,");
    }
    sqlPoint.pop_back(); // remove trailing comma
    sqlPoint += ")";

    return sqlPoint;
}
