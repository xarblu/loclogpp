#pragma once

#include "point.hpp"

#include <optional>

namespace LocLogPP {

/**
 * Exponential Moving Average filter for Points
 */
class EMAFilter {
    /**
     * Last smoothed Point for EMA filter
     */
    std::optional<Point> m_lastSmoothedPoint{std::nullopt};

    double m_stopSpeedThreshold;

public:
    /**
     * Construct EMAFilter with the given stopSpeedThreshold
     */
    explicit EMAFilter(double stopSpeedThreshold)
        : m_stopSpeedThreshold{stopSpeedThreshold}
    {}

    /**
     * Apply Exponential Moving Average smoothing filter on the given Point
     */
    void apply(Point &point);
};

}
