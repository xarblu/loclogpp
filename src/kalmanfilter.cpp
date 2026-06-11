#include "kalmanfilter.hpp"

#include <chrono>

double LocLogPP::KalmanFilter::update(LocLogPP::Measurement &measurement) {
    const auto &timestamp = measurement.timestamp;
    const auto &position = measurement.position;
    const auto &speed = measurement.speed;
    const auto &errorMultiplier = measurement.errorMultiplier;

    // temporaries for "atomic" struct swaps
    ErrorCovariance tempErrorCovariance;
    State tempState;

    // scale uncertainty with time between measurements
    // to avoid "overcorrecting" future points after a blackout
    double deltaTime = std::chrono::duration<double>{timestamp - m_lastMeasurement}.count();
    if (deltaTime <= 0.0) deltaTime = 0.1;
    m_lastMeasurement = timestamp;

    // PREDICT step

    // predict state transition
    m_state.pos = m_state.pos + m_state.vel * deltaTime;

    // predict uncertainty
    tempErrorCovariance = {
        .p00 = m_errorCovariance.p00 + (m_errorCovariance.p01 + m_errorCovariance.p10) * deltaTime + m_errorCovariance.p11 * deltaTime * deltaTime + m_processNoiseCovariance.pos * deltaTime,
        .p01 = m_errorCovariance.p01 + m_errorCovariance.p11 * deltaTime,
        .p10 = m_errorCovariance.p10 + m_errorCovariance.p11 * deltaTime,
        .p11 = m_errorCovariance.p11 + m_processNoiseCovariance.vel * deltaTime,
    };
    m_errorCovariance = tempErrorCovariance;


    // CORRECT step

    // dynamic error based on current sensor errors
    const double adjustedSensorNoise{m_sensorNoiseCovariance * errorMultiplier};

    struct {
        double pos;
        double vel;
    } kalmanGain = {
        .pos = m_errorCovariance.p00 / (m_errorCovariance.p00 + adjustedSensorNoise),
        .vel = m_errorCovariance.p10 / (m_errorCovariance.p00 + adjustedSensorNoise),
    };
    
    // correct state
    tempState = {
        .pos = m_state.pos + kalmanGain.pos * (position - m_state.pos),
        .vel = m_state.vel + kalmanGain.vel * (position - m_state.pos),
    };
    m_state = tempState;

    // update uncertainty
    tempErrorCovariance = {
        .p00 = (1.0 - kalmanGain.pos) * m_errorCovariance.p00,
        .p01 = (1.0 - kalmanGain.pos) * m_errorCovariance.p01,
        .p10 = m_errorCovariance.p10 - kalmanGain.vel * m_errorCovariance.p00,
        .p11 = m_errorCovariance.p11 - kalmanGain.vel * m_errorCovariance.p01,
    };
    m_errorCovariance = tempErrorCovariance;

    return m_state.pos;
}
