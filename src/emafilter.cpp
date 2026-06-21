#include "emafilter.hpp"

#include "point.hpp"

#include <optional>


void LocLogPP::EMAFilter::apply(Point &point) {
    // init
    if (!m_lastSmoothedPoint) {
        m_lastSmoothedPoint = point;
        return;
    }

    // average speed between this and last point to avoid jitter
    const double speed{(point.speed() + m_lastSmoothedPoint->speed()) / 2.0};

    // dynamic alpha based on speed
    // lower speed means higher smoothing
    double alpha{1.0};

    const double &stopSpeed = m_stopSpeedThreshold;
    if (speed < stopSpeed) {
        // heavy smoothing while stationary
        alpha = 0.25;
    } else if (speed < 4.0) {
        // linear increase up to 4 m/s (~15 km/h)
        // low speeds have higher smoothing
        alpha = 0.25 + (0.75 * ((speed - stopSpeed) / (4.0 - stopSpeed)));
    } else {
        // no smoothing for anything faster
        // because points should be far enough apart
        alpha = 1.0;
    }

    // apply EMA to lat, lon and speed
    point.setLatitude((alpha * point.latitude()) + ((1.0 - alpha) * m_lastSmoothedPoint->latitude()));
    point.setLongitude((alpha * point.longitude()) + ((1.0 - alpha) * m_lastSmoothedPoint->longitude()));
    point.setSpeed((alpha * point.speed()) + ((1.0 - alpha) * m_lastSmoothedPoint->speed()));

    // alt is optional and not on every Point
    // only smooth when both points have a value
    // if we have an altitude but the new point doesn't
    // "smooth" it by just copying to work around temporary loss
    // of 3D fix
    if (point.altitude() && m_lastSmoothedPoint->altitude()) {
        point.setAltitude((alpha * *point.altitude()) + ((1.0 - alpha) * *m_lastSmoothedPoint->altitude()));
    } else if (!point.altitude() && m_lastSmoothedPoint->altitude()) {
        point.setAltitude(*m_lastSmoothedPoint->altitude());
    }

    m_lastSmoothedPoint = point;
}
