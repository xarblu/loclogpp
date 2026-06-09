#include "point.hpp"

#include "logger.hpp"
#include "argparser.hpp"

#include <gps.h>
#include <libgpsmm.h>
#include <sqlite3.h>

#include <string>
#include <format>
#include <cmath>
#include <sstream>
#include <cstdint>
#include <chrono>

static inline std::string unixSecondsToISO8601UTC(std::int64_t unixSeconds) {
    std::chrono::system_clock::time_point tp{std::chrono::seconds{unixSeconds}};
    return std::format("{:%FT%TZ}", tp);
}

std::optional<LocLogPP::Point> LocLogPP::Point::fromGPSD(gps_data_t &data, ArgParser *args) {
    if (!(data.set & MODE_SET)) {
        Logger::debug("Point rejected: fix.mode is required");
        return std::nullopt;
    }

    const char* fixModeStr;
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

    if (data.fix.mode < MODE_2D) {
        Logger::debug("Point rejected: fix.mode must be at least MODE_2D (is {})", fixModeStr);
        return std::nullopt;
    }

    // reject low satellite count
    constexpr int satellitesRequired{5};
    if (data.satellites_used < satellitesRequired) {
        Logger::debug("Point rejected: Low satellite count (has {}, want >{})", data.satellites_used, satellitesRequired);
        return std::nullopt;
    }

    Point point{};

    // TIMESTAMP
    if (!(data.set & TIME_SET)) {
        Logger::debug("Point rejected: fix.time is required");
        return std::nullopt;
    }
    point.m_timestamp += std::chrono::seconds{data.fix.time.tv_sec};
    point.m_timestamp += std::chrono::microseconds{data.fix.time.tv_nsec / 1000};

    // HDOP
    if (!std::isfinite(data.dop.hdop)) {
        Logger::debug("Point rejected: dop.hdop is required");
        return std::nullopt;
    }
    if (data.dop.hdop > args->maxHDOP()) {
        Logger::debug("Point rejected: Bad HDOP (has {:.3f}, max {:.3f})", data.dop.hdop, args->maxHDOP());
        return std::nullopt;
    }
    point.m_hdop = data.dop.hdop;

    // LAT + LON
    if (!(data.set & LATLON_SET) && std::isfinite(data.fix.latitude) && std::isfinite(data.fix.longitude)) {
        Logger::debug("Point rejected: fix.latitude and fix.longitude are required");
        return std::nullopt;
    }
    point.m_latitude = data.fix.latitude;
    point.m_longitude = data.fix.longitude;

    // ALT + VDOP
    if ((data.set & ALTITUDE_SET) && std::isfinite(data.fix.altMSL)) {
        // altidude above mean sea level
        // this is what Owntracks uses as well
        // https://owntracks.org/booklet/tech/json/#_typelocation
        point.m_altitude = data.fix.altMSL;

        if (std::isfinite(data.dop.vdop)) {
            point.m_vdop = data.dop.vdop;
        }
    }
    if (std::isfinite(data.dop.vdop) && data.dop.vdop > args->maxVDOP()) {
        Logger::debug("Point altitude discarded: Bad VDOP (has {:.3f}, max {:.3f})", data.dop.vdop, args->maxVDOP());
        point.m_altitude.reset();
    }
    if (point.m_altitude && *point.m_altitude < args->minAltitudeMeters()) {
        Logger::debug("Point rejected: Altitude below minimum (has {:.3f}, min {:.3f})", *point.m_altitude, args->minAltitudeMeters());
        return std::nullopt;
    }

    // SPEED
    if ((data.set & SPEED_SET) && std::isfinite(data.fix.speed)) {
        point.m_speed = data.fix.speed;
    }

    // ACCURACY
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

    point.m_timestamp = std::chrono::system_clock::time_point{std::chrono::seconds{sqlite3_column_int64(statement, 1)}};
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

    double haversineLat = std::sin(deltaLat / 2.0) * std::sin(deltaLat / 2.0);
    double haversineLon = std::sin(deltaLon / 2.0) * std::sin(deltaLon / 2.0);

    double haversine = haversineLat + std::cos(latA) * std::cos(latB) * haversineLon;
    
    double distance = 2.0 * EARTH_RADIUS_METERS * std::atan2(std::sqrt(haversine), std::sqrt(1.0 - haversine));

    return distance;
}

void LocLogPP::Point::update(const Point &other) {
    m_timestamp = other.timestamp();
    m_latitude = other.latitude();
    m_longitude = other.longitude();
    if (other.speed()) m_speed = other.speed();
    if (other.accuracy()) m_accuracy = other.accuracy();
    if (other.altitude()) m_altitude = other.altitude();
}

std::string LocLogPP::Point::toString() const {
    return std::format(
        "timestamp: {:%FT%TZ}\n"
        "latitude: {}\n" 
        "longitude: {}\n"
        "altitude: {}\n" 
        "speed: {}\n"
        "accuracy: {}\n"
        "hdop: {}\n"
        "vdop: {}",
        m_timestamp,
        m_latitude,
        m_longitude,
        m_altitude.value_or(NAN),
        m_speed.value_or(NAN),
        m_accuracy.value_or(NAN),
        m_hdop,
        m_vdop.value_or(NAN));
}

std::string LocLogPP::Point::toSQL() const {
    std::string sqlPoint{"(NULL,"};
    sqlPoint += std::format(" {},", std::chrono::duration_cast<std::chrono::seconds>(m_timestamp.time_since_epoch()).count());
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

std::string LocLogPP::Point::toGPX() const {
    std::stringstream gpx{};

    gpx << "      <trkpt lat=\"" << m_latitude << "\" lon=\"" << m_longitude << "\">\n";
    if (m_altitude) {
        gpx << "        <ele>" << m_altitude.value() << "</ele>\n";
    }
    if (m_speed) {
        gpx << "        <speed>" << m_speed.value() << "</speed>\n";
    }
    gpx << "        <time>" << std::format("{:%FT%TZ}", m_timestamp) << "</time>\n";
    gpx << "      </trkpt>\n";

    return std::move(gpx.str());
}
