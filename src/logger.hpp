#pragma once

#include <chrono>
#include <ctime>
#include <format>
#include <iostream>
#include <mutex>
#include <type_traits>
#include <utility>


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
