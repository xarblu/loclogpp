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

    while (true) {
        auto point = geolocator->awaitPoint();
        if (!point) {
            std::cerr << "Got no point\nAssuming GPSD connection died\n";
            return 1;
        }

        std::cout << "Got point:\n" << point->toString();
        db->addPoint(point.value());
    }

    return 0;
}
