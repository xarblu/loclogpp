#pragma once

#include "utils.hpp"

#include <chrono>

namespace LocLogPP {

/**
 * Measurement for the KalmanFilter
 */
struct Measurement {
    // timestamp of the measurement
    std::chrono::system_clock::time_point timestamp{};

    // position of the value (lat/lon/alt)
    // lat/lon: deg
    // alt: m
    double position{};
    // lat/lon: deg^2
    // alt: m^2
    double positionError{};

    // "change rate" of the value (speed/climb)
    // lat/lon: deg/s
    // alt: m/s
    double speed{};
    // lat/lon: (deg/s)^2
    // alt: (m/s)^2
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
     * The default is intended for lat/lon measurements in deg.
     * Adjust this up to allow sharper turns or down for smoother/straighter lines
     * Should be increased when cutting too many curves/corners
     *
     * processNoiseVel (~ inertia of the movement)
     * The default is intended for lat/lon measurements in deg.
     * Adjust this up to adjust speed of state faster or down to adjust slower
     * Should be increased when overshooting curves/corners
     *
     * sensorNoise (~ scalar for the Measurement provided *Error values)
     * Baseline of 1.0 means fully trusting the Measurement error as is
     * Lower values mean "lean towards measurement"
     * Higher values mean "lean towards prediction"
     */
    explicit KalmanFilter(Measurement &seed,
                          double processNoisePos = metToDegSquared(0.1),
                          double processNoiseVel = metToDegSquared(3.0),
                          double sensorNoise = 1.0)
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
