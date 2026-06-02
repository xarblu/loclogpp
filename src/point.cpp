#include "point.hpp"

#include <gps.h>
#include <libgpsmm.h>

#include <string>
#include <format>
#include <iostream>

std::optional<LocLogPP::Point> LocLogPP::Point::fromGPSD(gps_data_t &data) {
    if (!(data.set & MODE_SET)) {
        std::cerr << "fix.mode is required\n";
        return std::nullopt;
    }

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
