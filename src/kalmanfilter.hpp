#pragma once

#include <chrono>

namespace LocLogPP {
/**
 * Basic 2D Kalman filter for Geolocator
 * to be used on individual Point location components
 * (mainly lat, lon, alt)
 */
class KalmanFilter {
    struct {
        double pos{};
        double vel{};
    }  m_processNoiseCovariance;

    double m_sensorNoiseCovariance{};

    struct State {
        double pos{0.0};
        double vel{0.0};
    } m_state;

    struct ErrorCovariance {
        // pos uncertainty
        double p00{1.0};
        // correlations
        double p01{0.0};
        double p10{0.0};
        // vel uncertainty
        double p11{1.0};
    } m_errorCovariance;

    std::chrono::system_clock::time_point m_lastMeasurement{};
public:
    /**
     * seed - initial state taken as-is
     *
     * processNoiseCovariancePos (~ position jitter)
     * adjust this up to allow sharper turns or down for smoother/straighter lines
     * should be increased when cutting too many curves/corners
     *
     * processNoiseCovarianceVel (~ inertia of the movement)
     * adjust this up to adjust speed of state faster or down to adjust slower
     * should be increased when overshooting curves/corners
     *
     * sensorNoiseCovariance
     * higher -> filter larger spikes
     */
    explicit KalmanFilter(double seedPos,
                          std::chrono::system_clock::time_point seedTime,
                          double processNoiseCovariancePos = 0.00001,
                          double processNoiseCovarianceVel = 0.000001,
                          double sensorNoiseCovariance = 0.005)
        : m_state{.pos = seedPos, .vel = 0.0}
        , m_lastMeasurement{seedTime}
        , m_processNoiseCovariance{.pos = processNoiseCovariancePos, .vel = processNoiseCovarianceVel}
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
