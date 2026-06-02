#include "database.hpp"
#include "geolocator.hpp"

#include <iostream>
#include <format>

int main(int argc, char* argv[]) {
    auto db = LocLogPP::Database::open("./db.sqlite3");
    if (!db) {
        std::cerr << "Database init failed\n";
        return 1;
    }
    std::cerr << "Database initialized\n";

    auto geolocator = LocLogPP::Geolocator::create();
    if (!geolocator) {
        std::cerr << "Geolocator init failed\n";
        return 1;
    }
    std::cerr << "Geolocator initialized\n";


    std::optional<LocLogPP::Point> prevPoint{std::nullopt};

    while (true) {
        auto point = geolocator->awaitPoint();
        if (!point) {
            std::cerr << "Got no point\nAssuming GPSD connection died\n";
            return 1;
        }

        std::cout << "Got point:\n" << point->toString();
        if (point->accuracy()) {
            auto accuracy = point->accuracy().value();
            if (accuracy > 1000.0) {
                std::cerr << std::format("Point accuracy insufficient ({} m)\n", accuracy);
                continue;
            }
            std::cerr << std::format("Point accuracy sufficient ({} m)\n", accuracy);
        }

        if (prevPoint) {
            auto distance = prevPoint->distance(*point);
            if (distance < 5.0) {
                std::cerr << std::format("Distance to last point insufficient ({} m)\n", distance);
                continue;
            }
            std::cerr << std::format("Distance to last point sufficient ({} m)\n", distance);
        }

        db->addPoint(point.value());
        prevPoint = point;
    }

    return 0;
}
