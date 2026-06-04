#include "logger.hpp"

#include "argparser.hpp"

#include <memory>

// logger singleton
std::unique_ptr<LocLogPP::Logger> LocLogPP::Logger::s_logger{nullptr};

LocLogPP::Logger* LocLogPP::Logger::instance() {
    if (!s_logger) {
        s_logger = std::make_unique<Logger>();
    }

    return s_logger.get();
}

bool LocLogPP::Logger::shouldPrint(LocLogPP::Logger::Priority priority) const {
    if (!m_args) [[unlikely]] {
        return true;
    }

    if (priority <= m_args->logLevel()) {
        return true;
    }

    return false;
}

void LocLogPP::Logger::attachConfig(std::shared_ptr<ArgParser> args) {
    instance()->m_args = args;
}

std::optional<LocLogPP::Logger::Priority> LocLogPP::Logger::priorityFromString(const std::string &priorityStr) {
    if (priorityStr == "debug" || priorityStr == "DEBUG") {
        return Priority::DEBUG;
    }

    if (priorityStr == "info" || priorityStr == "INFO") {
        return Priority::INFO;
    }

    if (priorityStr == "warn" || priorityStr == "WARN") {
        return Priority::WARN;
    }

    if (priorityStr == "error" || priorityStr == "ERROR") {
        return Priority::ERROR;
    }

    return std::nullopt;
}

std::string LocLogPP::Logger::priorityToString(LocLogPP::Logger::Priority priority) {
    switch (priority) {
        case Priority::DEBUG:
            return "DEBUG";
        case Priority::INFO:
            return "INFO";
        case Priority::WARN:
            return "WARN";
        case Priority::ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}
