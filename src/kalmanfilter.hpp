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
     * sensorNoiseCovariance (~ base sensor error under "perfect" conditions)
     * higher -> filter larger spikes
     * note that this gets multiplied with a dynamic error each update()
     */
    explicit KalmanFilter(double seedPos,
                          std::chrono::system_clock::time_point seedTime,
                          double processNoiseCovariancePos = 0.0001,
                          double processNoiseCovarianceVel = 0.00005,
                          double sensorNoiseCovariance = 0.0005)
        : m_state{.pos = seedPos, .vel = 0.0}
        , m_lastMeasurement{seedTime}
        , m_processNoiseCovariance{.pos = processNoiseCovariancePos, .vel = processNoiseCovarianceVel}
        , m_sensorNoiseCovariance{sensorNoiseCovariance}
    {}

    /**
     * Update the filter and get the latest state
     * 
     * timestamp affects how much we trust the prediction,
     * depending on how far back the prvious point was
     * (larger -> lower trust)
     *
     * measurement is the sensor provided data
     *
     * errorMultiplier is multiplied onto m_sensorNoiseCovariance
     * before calculating Kalman gains and should be made up of:
     * LAT/LON:
     *   - {X,Y}DOP
     *   - EP{X,Y}
     *   - EPS
     * ALT:
     *   - VDOP
     *   - EPV
     * (larger -> lower trust)
     */
    double update(std::chrono::system_clock::time_point timestamp, double measurement, double errorMultiplier = 1.0);
};

} // namespace LocLogPP
