#include "kalmanfilter.hpp"

double LocLogPP::KalmanFilter::update(double measurement, double dop) {
    // PREDICT step
    // since were filtering in 1D without any speed info
    // we'll predict the next state to match the current one
    m_errorCovariance += m_processNoiseCovariance;

    // CORRECT step

    // dynamic error based on receiver dilution of precision
    double adjustedSensorNoise{m_sensorNoiseCovariance * dop * dop};

    m_kalmanGain = m_errorCovariance / (m_errorCovariance + adjustedSensorNoise);
    
    m_state += m_kalmanGain * (measurement - m_state);

    m_errorCovariance = (1.0 - m_kalmanGain) * m_errorCovariance;

    return m_state;
}
