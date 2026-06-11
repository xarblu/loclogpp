#include "kalmanfilter.hpp"

#include <chrono>

double LocLogPP::KalmanFilter::update(LocLogPP::Measurement &measurement) {
    // temporaries for "atomic" struct swaps
    Error tempError;
    State tempState;

    // scale uncertainty with time between measurements
    // to avoid "overcorrecting" future points after a blackout
    double deltaTime = std::chrono::duration<double>{measurement.timestamp - m_lastMeasurement}.count();
    if (deltaTime <= 0.0) deltaTime = 0.1;
    m_lastMeasurement = measurement.timestamp;

    // PREDICT step

    // predict state transition
    m_state.pos = m_state.pos + m_state.vel * deltaTime;

    // predict uncertainty
    tempError= {
        .p00 = m_error.p00 + (m_error.p01 + m_error.p10 + m_error.p11 * deltaTime) * deltaTime + m_processNoise.pos * deltaTime,
        .p01 = m_error.p01 + m_error.p11 * deltaTime,
        .p10 = m_error.p10 + m_error.p11 * deltaTime,
        .p11 = m_error.p11 + m_processNoise.vel * deltaTime,
    };
    m_error = tempError;


    // CORRECT step

    // measurement "innovation"
    struct {
        const double pos;
        const double vel;
    } innovation{
        .pos = measurement.position - m_state.pos,
        .vel = measurement.speed - m_state.vel,
    };

    // measurement uncertainty
    struct {
        const double pos;
        const double vel;
    } uncertainty{
        .pos = std::max(1e-12, m_sensorNoise * measurement.positionError),
        .vel = std::max(1e-12, measurement.speedError),
    };

    // det(H * P * H^T + R)
    const double det{
        (m_error.p00 + uncertainty.pos)
        * (m_error.p11 + uncertainty.vel)
        - (m_error.p01 * m_error.p10)};

    // singularity safeguard, just bail with the predicted state
    if (std::abs(det) < 1e-15) return m_state.pos;

    // Kalman gain matrix
    struct {
        double k00;
        double k01;
        double k10;
        double k11;
    } gain{
        .k00 = (m_error.p00 * (m_error.p11 + uncertainty.vel) - m_error.p01 * m_error.p10) / det,
        .k01 = (m_error.p01 * uncertainty.pos) / det,
        .k10 = (m_error.p10 * uncertainty.vel) / det,
        .k11 = ((m_error.p00 + uncertainty.pos) * m_error.p11 - m_error.p10 * m_error.p01) / det,
    };
    
    // correct state
    tempState = {
        .pos = m_state.pos + gain.k00 * innovation.pos + gain.k01 * innovation.vel,
        .vel = m_state.vel + gain.k10 * innovation.pos + gain.k11 * innovation.vel,
    };
    m_state = tempState;

    // update uncertainty
    tempError= {
        .p00 = (1.0 - gain.k00) * m_error.p00 - gain.k01 * m_error.p10,
        .p01 = (1.0 - gain.k00) * m_error.p01 - gain.k01 * m_error.p11,
        .p10 = -gain.k10 * m_error.p00 + (1.0 - gain.k11) * m_error.p10,
        .p11 = -gain.k10 * m_error.p01 + (1.0 - gain.k11) * m_error.p11,
    };
    m_error = tempError;

    return m_state.pos;
}
