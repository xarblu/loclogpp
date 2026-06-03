#pragma once

#include <format>
#include <mutex>
#include <utility>
#include <type_traits>
#include <chrono>
#include <iostream>


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
    // logger singleton
    static std::unique_ptr<LocLogPP::Logger> s_logger;

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
    void logInternal(Priority priority, std::format_string<std::type_identity_t<Args>...> fmt, Args&&... args) {
        std::string timestamp = std::format("[{:%F %T}] ", std::chrono::system_clock::now());
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

    std::string message = std::format(fmt, std::forward<Args>(args)...);

    {
        std::lock_guard lock{m_loggerMutex};
        std::cerr << timestamp << priorityStr << message << "\n";
    }

    }

public:
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
};

} // namespace LocLogPP
