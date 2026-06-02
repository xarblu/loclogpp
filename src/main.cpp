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

        std::cout << "Got point\n";
        if (prevPoint) {
            if (auto distance = prevPoint->distance(*point); distance < 5.0) {
                std::cerr << std::format("Distance to last point insufficient ({}m)\n", distance);
                continue;
            }
        }

        db->addPoint(point.value());
        prevPoint = point;
    }

    return 0;
}
