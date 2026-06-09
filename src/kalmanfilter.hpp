#pragma once

#include <chrono>

namespace LocLogPP {
/**
 * Basic 1D Kalman filter for Geolocator
 * to be used on individual Point location components
 * (mainly lat, lon, alt)
 */
class KalmanFilter {
    // kalman
    double m_processNoiseCovariance{};
    double m_sensorNoiseCovariance{};
    double m_state{0.0};
    double m_errorCovariance{1.0};
    double m_kalmanGain{};

    // extras
    std::chrono::system_clock::time_point m_lastMeasurement{};
public:
    /**
     * seed - initial state taken as-is
     * processNoiseCovariance - lower -> smoother path, but slower reaction
     * sensorNoiseCovariance - higher -> filter larger spikes
     */
    explicit KalmanFilter(double seed,
                          std::chrono::system_clock::time_point seedTime,
                          double processNoiseCovariance = 0.0001,
                          double sensorNoiseCovariance = 0.005)
        : m_state{seed}
        , m_lastMeasurement{seedTime}
        , m_processNoiseCovariance{processNoiseCovariance}
        , m_sensorNoiseCovariance{sensorNoiseCovariance}
    {}

    /**
     * Update the filter and get the latest state
     * 
     * dop affects how much we trust the given measurement
     * (larger -> lower trust)
     * 
     * timestamp affects how much we trust the prediction,
     * depending on how far back the prvious point was
     * (larger -> lower trust)
     */
    double update(double measurement, double dop, std::chrono::system_clock::time_point timestamp);
};

} // namespace LocLogPP
