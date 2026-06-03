#pragma once

#include <format>
#include <memory>
#include <mutex>

namespace LocLogPP {

class Logger {
public:
    enum class Priority {
        DEBUG,
        INFO,
        WARN,
        ERROR,
    };

private:
    std::mutex m_loggerMutex{};

    /**
     * Get pointer to the logger singleton
     * Will create it if necessary
     */
    static Logger* instance();

    /**
     * Internal log printer
     * Ensures synchronised output
     */
    template<class... Args>
    void logInternal(Priority priority, std::format_string<Args...> fmt, Args&&... args);

public:
    /**
     * Generic log function
     */
    template<class... Args>
    static void log(Priority priority, std::format_string<Args...> fmt, Args&&... args);

    /**
     * Helpers for the individual priorities
     */
    template<class... Args>
    static inline void debug(std::format_string<Args...> fmt, Args&&... args) { log(Priority::DEBUG, fmt, args...); }

    template<class... Args>
    static inline void info(std::format_string<Args...> fmt, Args&&... args) { log(Priority::INFO, fmt, args...); }

    template<class... Args>
    static inline void warn(std::format_string<Args...> fmt, Args&&... args) { log(Priority::WARN, fmt, args...); }

    template<class... Args>
    static inline void error(std::format_string<Args...> fmt, Args&&... args) { log(Priority::ERROR, fmt, args...); }
};

// logger singleton
static std::unique_ptr<Logger> s_logger{nullptr};

} // namespace LocLogPP
