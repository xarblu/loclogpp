#include "point.hpp"

#include <gps.h>
#include <libgpsmm.h>

#include <string>
#include <format>

std::optional<LocLogPP::Point> LocLogPP::Point::fromGPSD(gps_data_t &data) {
    Point point{};

    if (data.set & TIME_SET) {
        point.m_timestamp = data.fix.time.tv_sec;
    } else {
        return std::nullopt;
    }

    if (data.set & LATLON_SET) {
        point.m_latitude = data.fix.latitude;
        point.m_longitude = data.fix.longitude;
    } else {
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
        timestamp(),
        latitude(),
        longitude(),
        altitude().value_or(NAN),
        speed().value_or(NAN),
        accuracy().value_or(NAN));
}
