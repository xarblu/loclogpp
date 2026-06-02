#include "database.hpp"
#include "geolocator.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    auto db = LocLogPP::Database::open("./db.sqlite3");
    if (!db) {
        std::cerr << "Database init failed\n";
        return 1;
    }

    auto geolocator = LocLogPP::Geolocator::create();
    if (!geolocator) {
        std::cerr << "Geolocator init failed\n";
        return 1;
    }

    auto point = geolocator->awaitPoint();
    if (point) {
        std::cout << "Got point:\n" << point->toString();
    } else {
        return 1;
    }

    return 0;
}
