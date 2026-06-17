#pragma once

#include "logger.hpp"

#include <libgpsmm.h>

#include <memory>
#include <string>
#include <chrono>

namespace LocLogPP {

// main operation to perform
enum class Operation {
    // track points
    TRACK,

    // export database
    EXPORT,
};

class ArgParser {
    // global
    Operation m_operation{};
    std::string m_dbFile{"./db.sqlite3"};
    Logger::Priority m_logLevel{Logger::Priority::INFO};
    bool m_useSyslogPrefix{false};

    // track
    std::string m_gpsdHost{"localhost"};
    std::string m_gpsdPort{DEFAULT_GPSD_PORT};
    int m_pointIntervalSeconds{5};
    int m_stationaryHeartbeatSeconds{300};
    double m_requiredAccuracyMeters{200.0};
    double m_requiredDistanceMeters{5.0};
    double m_minAltitudeMeters{-5.0};
    int m_maxSpeedMetersPerSecond{100};
    // these defaults come from what this site considers "good"
    // https://www.marinepublic.com/blogs/training/649465-hdop-pdop-gdop-gps-accuracy-multipath-effects
    double m_maxHDOP{5.0};
    double m_maxVDOP{3.0};
    std::chrono::seconds m_fixWarmup{15};

    /**
     * Print help message
     */
    static void printHelp();

    // use parse()
    ArgParser() = default;

public:
    /**
     * Parse the args
     * Success: {0, ArgParser*}
     * Error: {>0, nullptr}
     * No Op (--help): {0, nullptr}
     */
    static std::pair<int, std::shared_ptr<ArgParser>> parse(int argc, char* argv[]);

    /**
     * Getters
     */
    Operation operation() const { return m_operation; }
    std::string dbFile() const { return m_dbFile; }
    Logger::Priority logLevel() const { return m_logLevel; }
    bool useSyslogPrefix() const { return m_useSyslogPrefix; }
    std::string gpsdHost() const { return m_gpsdHost; };
    std::string gpsdPort() const { return m_gpsdPort; };
    int pointIntervalSeconds() const { return m_pointIntervalSeconds; }
    int stationaryHeartbeatSeconds() const { return m_stationaryHeartbeatSeconds; }
    double requiredAccuracyMeters() const { return m_requiredAccuracyMeters; }
    double requiredDistanceMeters() const { return m_requiredDistanceMeters; }
    double minAltitudeMeters() const { return m_minAltitudeMeters; }
    int maxSpeedMetersPerSecond() const { return m_maxSpeedMetersPerSecond; }
    double maxHDOP() const { return m_maxHDOP; }
    double maxVDOP() const { return m_maxVDOP; }
    std::chrono::seconds fixWarmup() { return m_fixWarmup; };
};

} //namespace LocLogPP
