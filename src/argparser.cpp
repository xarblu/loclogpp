#include "argparser.hpp"

#include "logger.hpp"

#include <string_view>
#include <memory>
#include <iostream>

void LocLogPP::ArgParser::printHelp() {
    std::cerr << "Usage: loclogpp {GLOBAL_OPT...} OPERATION {OPERATION_OPT...}\n"
              << "\n"
              << "GLOBAL_OPTs:\n"
              << "  -h|--help     Print this help message\n"
              << "  -d|--db-file  SQLite3 database file\n"
              << "\n"
              << "OPERATIONs:\n"
              << "  track         Run the tracker\n"
              << "  export        Export collected points\n"
              << "\n"
              << "OPERATION_OPTs for track:\n"
              << "\n"
              << "OPERATION_OPTs for export:\n"
              ;
}

std::pair<int, std::unique_ptr<LocLogPP::ArgParser>> LocLogPP::ArgParser::parse(int argc, char* argv[]) {
    auto parser = std::unique_ptr<ArgParser>(new ArgParser());

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
        auto arg = std::string_view{argv[i]};

        Logger::error("Unknown OPERATION_OPT: {}", arg);
        return {1, nullptr};
    }

    return {0, std::move(parser)};
}
