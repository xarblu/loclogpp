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

std::string LocLogPP::Logger::logPrefix(LocLogPP::Logger::Priority priority) const {
    if (m_args && m_args->useSyslogPrefix()) {
        return std::format("<{}>", static_cast<int>(priority));
    }

    // time
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
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

    return timeStr + priorityStr;
}

std::string LocLogPP::Logger::indentMessage(std::string &&message, size_t indent) const {
    if (m_args && m_args->useSyslogPrefix()) {
        return std::move(message) + "\n";
    }

    // message, indented by the prefix
    // (not needed if runnin as service - syslog strips the prefix)
    std::ostringstream messageOut{};
    std::istringstream messageIn{};
    messageIn.str(std::move(message));

    bool first{true};
    for (std::string line; std::getline(messageIn, line);) {
        if (first) {
            messageOut << line << "\n";
            first = false;
            continue;
        }
        messageOut << std::string(indent, ' ') << line << "\n";
    }

    return std::move(messageOut.str());
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
