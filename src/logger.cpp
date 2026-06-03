#include "logger.hpp"

#include <memory>
#include <format>
#include <chrono>
#include <mutex>
#include <iostream>
#include <string>

LocLogPP::Logger* LocLogPP::Logger::instance() {
    if (!s_logger) {
        s_logger = std::make_unique<Logger>();
    }

    return s_logger.get();
}

template<class... Args>
void LocLogPP::Logger::logInternal(Priority priority, std::format_string<Args...> fmt, Args&&... args) {
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

    std::string message = std::format(fmt, args...);

    {
        std::lock_guard lock{m_loggerMutex};
        std::cerr << timestamp << priorityStr << message << "\n";
    }
}

template<class... Args>
void LocLogPP::Logger::log(Priority priority, std::format_string<Args...> fmt, Args&&... args) {
    auto logger = Logger::instance();
    logger->logInternal(priority, fmt, args...);
}
