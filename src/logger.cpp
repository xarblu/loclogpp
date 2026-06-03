#include "logger.hpp"

#include <memory>

// logger singleton
std::unique_ptr<LocLogPP::Logger> LocLogPP::Logger::s_logger{nullptr};

LocLogPP::Logger* LocLogPP::Logger::instance() {
    if (!s_logger) {
        s_logger = std::make_unique<Logger>();
    }

    return s_logger.get();
}
