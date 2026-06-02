#include "point.hpp"

#include <gps.h>
#include <libgpsmm.h>

#include <string>
#include <format>
#include <iostream>
#include <cmath>

std::optional<LocLogPP::Point> LocLogPP::Point::fromGPSD(gps_data_t &data) {
    if (!(data.set & MODE_SET)) {
        std::cerr << "fix.mode is required\n";
        return std::nullopt;
    }

    std::cerr << "fix.mode is ";
    switch (data.fix.mode) {
        case MODE_NOT_SEEN:
            std::cerr << "MODE_NOT_SEEN";
            break;
        case MODE_NO_FIX:
            std::cerr << "MODE_NO_FIX";
            break;
        case MODE_2D:
            std::cerr << "MODE_2D";
            break;
        case MODE_3D:
            std::cerr << "MODE_3D";
            break;
        default:
            std::cerr << "UNKNOWN";
            break;
    }
    std::cerr << "\n";

    if (data.fix.mode < MODE_2D) {
        std::cerr << "fix.mode must be at least MODE_2D\n";
        return std::nullopt;
    }

    Point point{};

    if (data.set & TIME_SET) {
        point.m_timestamp = data.fix.time.tv_sec;
    } else {
        std::cerr << "fix.time is required\n";
        return std::nullopt;
    }

    if (data.set & LATLON_SET) {
        point.m_latitude = data.fix.latitude;
        point.m_longitude = data.fix.longitude;
    } else {
        std::cerr << "fix.latitude and fix.longitude are required\n";
        return std::nullopt;
    }

    if (data.set & ALTITUDE_SET) {
        // altidude above mean sea level
        // this is what Owntracks uses as well
        // https://owntracks.org/booklet/tech/json/#_typelocation
        point.m_altitude = data.fix.altMSL;
    }

    if (data.set & SPEED_SET) {
        point.m_speed = data.fix.speed;
    }

    if (data.set & HERR_SET) {
        // only horizontal accuracy for now
        point.m_accuracy = data.fix.eph;
    }

    return point;
}

double LocLogPP::Point::distance(Point &other) {
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
        "accuracy: {}\n",
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
