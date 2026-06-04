#pragma once

#include <memory>
#include <string>

namespace LocLogPP {

// main operation to perform
enum class Operation {
    // track points
    TRACK,

    // export database
    EXPORT,
};

class ArgParser {
    Operation m_operation{};
    std::string m_dbFile{"./db.sqlite3"};

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
    static std::pair<int, std::unique_ptr<ArgParser>> parse(int argc, char* argv[]);

    /**
     * Getters
     */
    Operation operation() const { return m_operation; }
    std::string dbFile() const { return m_dbFile; }
};

} //namespace LocLogPP
