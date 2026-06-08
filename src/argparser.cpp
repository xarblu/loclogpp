#include "argparser.hpp"

#include "logger.hpp"

#include <stdexcept>
#include <string_view>
#include <memory>
#include <iostream>

void LocLogPP::ArgParser::printHelp() {
    ArgParser defaults{};

    std::cerr << "Usage: loclogpp {GLOBAL_OPT...} OPERATION {OPERATION_OPT...}\n"
              << "\n"
              << "GLOBAL_OPTs:\n"
              << "  -h|--help               Print this help message\n"
              << "  -d|--db-file            SQLite3 database file (default: " << defaults.dbFile() << ")\n"
              << "  -l|--log-level          Log level [DEBUG,INFO,WARN,ERROR] (default: " << Logger::priorityToString(defaults.logLevel()) << ")\n"
              << "  --syslog                Enable syslog compatible prio-prefixes (<prio>Message) instead of our own\n"
              << "\n"
              << "OPERATIONs:\n"
              << "  track                   Run the tracker\n"
              << "  export                  Export collected points\n"
              << "\n"
              << "OPERATION_OPTs for track:\n"
              << "  --gpsd-host             GPSD host address (default: " << defaults.gpsdHost() << ")\n"
              << "  --gpsd-port             GPSD host port (default: " << defaults.gpsdPort() << ")\n"
              << "  --point-interval        Minimum point interval between points in seconds (default: " << defaults.pointIntervalSeconds() << ")\n"
              << "  --stationary-heartbeat  Point interval while stationary (default: " << defaults.stationaryHeartbeatSeconds() << ")\n"
              << "  --required-accuracy     Minimum accuracy required in meters (default: " << defaults.requiredAccuracyMeters() << ")\n"
              << "  --required-distance     Minimum distance required between points in meters (default: " << defaults.requiredDistanceMeters() << ")\n"
              << "  --max-speed             Maximum average speed between points in meters per second (default: " << defaults.maxSpeedMetersPerSecond() << ")\n"
              << "\n"
              << "OPERATION_OPTs for export:\n";
}

std::pair<int, std::shared_ptr<LocLogPP::ArgParser>> LocLogPP::ArgParser::parse(int argc, char* argv[]) {
    auto parser = std::shared_ptr<ArgParser>(new ArgParser());

    int i{1};

    // global flags
    for (; i < argc; i++) {
        auto arg = std::string_view{argv[i]};

        // end of global flags
        if (!arg.starts_with("-")) {
            break;
        }

        if (arg == "-h" || arg == "--help") {
            printHelp();
            return {0, nullptr};
        }

        if (arg == "-d" || arg == "--db-file") {
            if (i + 1 >= argc) {
                Logger::error("Argument requires value: {}", arg);
                return {1, nullptr};
            }
            parser->m_dbFile = argv[++i];
            continue;
        }

        if (arg == "-l" || arg == "--log-level") {
            if (i + 1 >= argc) {
                Logger::error("Argument requires value: {}", arg);
                return {1, nullptr};
            }
            auto level = Logger::priorityFromString(argv[++i]);
            if (!level) {
                Logger::error("Invalid value for: {}", arg);
                return {1, nullptr};
            }
            parser->m_logLevel = level.value();
            continue;
        }

        if (arg == "--syslog") {
            parser->m_useSyslogPrefix = true;
            continue;
        }

        Logger::error("Unknown GLOBAL_OPT: {}", arg);
        return {1, nullptr};
    }

    // operation
    if (i >= argc) {
        Logger::error("Expected OPERATION");
        return {1, nullptr};
    }
    do {
        auto arg = std::string_view{argv[i]};

        if (arg == "track") {
            parser->m_operation = Operation::TRACK;
            break;
        }

        if (arg == "export") {
            parser->m_operation = Operation::EXPORT;
            break;
        }

        Logger::error("Unknown OPERATION: {}", arg);
        return {1, nullptr};
    } while(false);
    i++;

    // operation specific flags
    for (; i < argc; i++) {
        switch (parser->operation()) {
            case Operation::TRACK:
                {
                    auto arg = std::string_view{argv[i]};

                    if (arg == "--gpsd-host") {
                        if (i + 1 >= argc) {
                            Logger::error("Argument requires value: {}", arg);
                            return {1, nullptr};
                        }
                        parser->m_gpsdHost = argv[++i];
                        continue;
                    }

                    if (arg == "--gpsd-port") {
                        if (i + 1 >= argc) {
                            Logger::error("Argument requires value: {}", arg);
                            return {1, nullptr};
                        }
                        parser->m_gpsdHost = argv[++i];
                        continue;
                    }

                    if (arg == "--point-interval") {
                        if (i + 1 >= argc) {
                            Logger::error("Argument requires value: {}", arg);
                            return {1, nullptr};
                        }
                        try {
                            parser->m_pointIntervalSeconds = std::stoi(std::string{argv[++i]});
                            continue;
                        } catch(std::invalid_argument e) {
                            Logger::error("Bad interger value for: {}", arg);
                            return {1, nullptr};
                        }
                    }

                    if (arg == "--stationary-heartbeat") {
                        if (i + 1 >= argc) {
                            Logger::error("Argument requires value: {}", arg);
                            return {1, nullptr};
                        }
                        try {
                            parser->m_stationaryHeartbeatSeconds = std::stoi(std::string{argv[++i]});
                            continue;
                        } catch(std::invalid_argument e) {
                            Logger::error("Bad interger value for: {}", arg);
                            return {1, nullptr};
                        }
                    }

                    if (arg == "--required-accuracy") {
                        if (i + 1 >= argc) {
                            Logger::error("Argument requires value: {}", arg);
                            return {1, nullptr};
                        }
                        try {
                            parser->m_requiredAccuracyMeters = std::stod(std::string{argv[++i]});
                            continue;
                        } catch(std::invalid_argument e) {
                            Logger::error("Bad float value for: {}", arg);
                            return {1, nullptr};
                        }
                    }

                    if (arg == "--required-distance") {
                        if (i + 1 >= argc) {
                            Logger::error("Argument requires value: {}", arg);
                            return {1, nullptr};
                        }
                        try {
                            parser->m_requiredDistanceMeters = std::stod(std::string{argv[++i]});
                            continue;
                        } catch(std::invalid_argument e) {
                            Logger::error("Bad float value for: {}", arg);
                            return {1, nullptr};
                        }
                    }

                    if (arg == "--max-speed") {
                        if (i + 1 >= argc) {
                            Logger::error("Argument requires value: {}", arg);
                            return {1, nullptr};
                        }
                        try {
                            parser->m_maxSpeedMetersPerSecond = std::stoi(std::string{argv[++i]});
                            continue;
                        } catch(std::invalid_argument e) {
                            Logger::error("Bad interger value for: {}", arg);
                            return {1, nullptr};
                        }
                    }

                    Logger::error("Unknown OPERATION_OPT: {}", arg);
                    return {1, nullptr};
                }
                break;

            default:
                Logger::error("Given OPERATION does not take any OPERATION_OPTs");
                return {1, nullptr};
        }
    }

    return {0, std::move(parser)};
}
