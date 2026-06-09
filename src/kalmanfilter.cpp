#include "kalmanfilter.hpp"

#include <chrono>

double LocLogPP::KalmanFilter::update(double measurement, double dop, std::chrono::system_clock::time_point timestamp) {
    // PREDICT step
    // since were filtering in 1D without any speed info
    // we'll predict the next state to match the current one
    
    // scale uncertainty with time between measurements
    // to avoid "overcorrecting" future points after a blackout
    const auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp - m_lastMeasurement);
    double deltaTimeSeconds = static_cast<double>(deltaTime.count()) / 1000.0;
    if (deltaTimeSeconds <= 0.0) deltaTimeSeconds = 1.0;

    m_errorCovariance += m_processNoiseCovariance * deltaTimeSeconds;

    // CORRECT step

    // dynamic error based on receiver dilution of precision
    const double adjustedSensorNoise{m_sensorNoiseCovariance * dop * dop};

    m_kalmanGain = m_errorCovariance / (m_errorCovariance + adjustedSensorNoise);
    
    m_state += m_kalmanGain * (measurement - m_state);

    m_errorCovariance = (1.0 - m_kalmanGain) * m_errorCovariance;

    return m_state;
}
