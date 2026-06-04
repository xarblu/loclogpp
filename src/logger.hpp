#pragma once

#include <chrono>
#include <ctime>
#include <format>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

namespace LocLogPP {

class ArgParser;

class Logger {
public:
    // some syslog style levels
    enum class Priority {
        DEBUG = 7,
        INFO = 6,
        WARN = 4,
        ERROR = 3,
    };

private:
    // logger singleton
    static std::unique_ptr<LocLogPP::Logger> s_logger;

    std::mutex m_loggerMutex{};

    std::shared_ptr<ArgParser> m_args{nullptr};

    /**
     * Get pointer to the logger singleton
     * Will create it if necessary
     */
    static Logger* instance();

    /**
     * Check whether we should print this mesage according to
     * configured priority
     */
    bool shouldPrint(Priority priority) const;

    /**
     * Internal log printer
     * Ensures synchronised output
     */
    template<class... Args>
    void logInternal(Priority priority, std::format_string<std::type_identity_t<Args>...> fmt, Args&&... args) {
        if (!shouldPrint(priority)) {
            return;
        }

        // std::localtime is not thread safe
        // let's just lock the entire function
        std::lock_guard lock{m_loggerMutex};

        // time
        std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::string timeStr{"[TIME ERROR] "};
        std::array<char, 32> buffer{};
        if (std::strftime(buffer.data(), buffer.size(), "[%F %T] ", std::localtime(&now)) > 0) {
            timeStr = buffer.data();
        }

        // priority
        std::string priorityStr;
        switch (priority) {
            case Priority::DEBUG:
                priorityStr = "[DEBUG] ";
                break;

            case Priority::INFO:
                priorityStr = "[INFO] ";
                break;
            case Priority::WARN:
                priorityStr = "[WARN] ";
                break;
            case Priority::ERROR:
                priorityStr = "[ERROR] ";
                break;
            default:
                priorityStr = "[UNKNOWN] ";
                break;
        }

        // message
        std::string message = std::format(fmt, std::forward<Args>(args)...);

        // send it
        std::cerr << timeStr << priorityStr << message << "\n";
    }

public:
    /**
     * Attach the shared config to the Logger singleton
     */
    static void attachConfig(std::shared_ptr<ArgParser> args);

    /**
     * Generic log function
     */
    template<class... Args>
    static void log(Priority priority, std::format_string<std::type_identity_t<Args>...> fmt, Args&&... args) {
        auto logger = Logger::instance();
        logger->logInternal(priority, fmt, std::forward<Args>(args)...);
    }

    /**
     * Helpers for the individual priorities
     */
    template<class... Args>
    static inline void debug(std::format_string<std::type_identity_t<Args>...> fmt, Args&&... args) { log(Priority::DEBUG, fmt, std::forward<Args>(args)...); }

    template<class... Args>
    static inline void info(std::format_string<std::type_identity_t<Args>...> fmt, Args&&... args) { log(Priority::INFO, fmt, std::forward<Args>(args)...); }

    template<class... Args>
    static inline void warn(std::format_string<std::type_identity_t<Args>...> fmt, Args&&... args) { log(Priority::WARN, fmt, std::forward<Args>(args)...); }

    template<class... Args>
    static inline void error(std::format_string<std::type_identity_t<Args>...> fmt, Args&&... args) { log(Priority::ERROR, fmt, std::forward<Args>(args)...); }

    /**
     * Log level string<->enum conversions helpers
     * priorityFromString returns nullopt on error
     */
    static std::optional<Priority> priorityFromString(const std::string &priorityStr);
    static std::string priorityToString(Priority priority);
};

} // namespace LocLogPP
