#pragma once

#include <utility>
#include <cmath>

/**
 * Globally average radius of Earth in meters
 * (according to https://en.wikipedia.org/wiki/Earth_radius)
 */
constexpr double EARTH_RADIUS_AVG_METERS{6371000.0};

/**
 * Radius of Earth around the equator in meters (from WGS-84)
 * (according to https://en.wikipedia.org/wiki/Earth_radius)
 */
constexpr double EARTH_RADIUS_EQUATORIAL_METERS{6378137.0};

/**
 * Eccentricity squared (e^2) (from WGS-84)
 * (according to https://en.wikipedia.org/wiki/World_Geodetic_System)
 *
 * Essentially how "squashed" the Earth ellipsoid is
 */
constexpr double EARTH_ECCENTRICITY_SQUARED{6.69437999014e-3};

/**
 * Multipliers to convert between radians and degrees
 */
constexpr double DEG_TO_RAD{M_PI / 180.0};
constexpr double RAD_TO_DEG{180.0 / M_PI};


/**
 * Calculate the speed in lat/lon directions in deg/sec based on
 * the given speed (m/s) and track direction at the given location
 */
static inline std::pair<double, double> speedToLatLonDegPerSecond(double latitude, double longitude, double speed, double track) {
    const double latRadians{latitude * DEG_TO_RAD};
    const double latRadiansSine{std::sin(latRadians)};

    const double denom{1.0 - EARTH_ECCENTRICITY_SQUARED * latRadiansSine * latRadiansSine};
    const double denomSqrt{std::sqrt(denom)};

    // earth radius at given location
    const double earthRadiusAtLat{(EARTH_RADIUS_EQUATORIAL_METERS * (1.0 - EARTH_ECCENTRICITY_SQUARED)) / (denom * denomSqrt)};
    const double earthRadiusAtLon{EARTH_RADIUS_EQUATORIAL_METERS / denomSqrt};

    // split speed
    const double trackRadians{track * DEG_TO_RAD};
    const double speedLatMPS{speed * std::cos(trackRadians)};
    const double speedLonMPS{speed * std::sin(trackRadians)};

    // convert to deg/s
    const double speedLatDPS{(speedLatMPS / earthRadiusAtLat) * RAD_TO_DEG};
    const double speedLonDPS{(speedLonMPS / (earthRadiusAtLon * std::cos(latRadians))) * RAD_TO_DEG};

    return {speedLatDPS, speedLonDPS};
}
