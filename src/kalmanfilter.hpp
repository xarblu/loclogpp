#pragma once

#include <chrono>

namespace LocLogPP {

/**
 * Measurement for the KalmanFilter
 */
struct Measurement {
    // timestamp of the measurement
    std::chrono::system_clock::time_point timestamp{};

    // position of the value (lat/lon/alt)
    double position{};
    double positionError{};

    // "change rate" of the value (speed/climb)
    double speed{};
    double speedError{};
};

/**
 * Basic 2D Kalman filter for Geolocator
 * to be used on individual Point location components
 * (mainly lat, lon, alt)
 */
class KalmanFilter {
    struct {
        double pos{};
        double vel{};
    }  m_processNoise;

    double m_sensorNoise{};

    struct State {
        double pos{0.0};
        double vel{0.0};
    } m_state;

    struct Error {
        // pos uncertainty
        double p00{1.0};
        // correlations
        double p01{0.0};
        double p10{0.0};
        // vel uncertainty
        double p11{1.0};
    } m_error;

    std::chrono::system_clock::time_point m_lastMeasurement{};
public:
    /**
     * seed - initial state taken as-is
     *
     * processNoisePos (~ position jitter)
     * adjust this up to allow sharper turns or down for smoother/straighter lines
     * should be increased when cutting too many curves/corners
     *
     * processNoiseVel (~ inertia of the movement)
     * adjust this up to adjust speed of state faster or down to adjust slower
     * should be increased when overshooting curves/corners
     *
     * sensorNoise (~ base sensor error under "perfect" conditions)
     * higher -> filter larger spikes
     * note that this gets multiplied with a dynamic error each update()
     */
    explicit KalmanFilter(Measurement &seed,
                          double processNoisePos = 0.0001,
                          double processNoiseVel = 0.00005,
                          double sensorNoise = 0.0005)
        : m_state{.pos = seed.position, .vel = seed.speed}
        , m_lastMeasurement{seed.timestamp}
        , m_processNoise{.pos = processNoisePos, .vel = processNoiseVel}
        , m_sensorNoise{sensorNoise}
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
     * errorMultiplier is multiplied onto m_sensorNoise
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
    double update(Measurement &measurement);
};

} // namespace LocLogPP
