#include "database.hpp"
#include "geolocator.hpp"
#include "logger.hpp"

#include <string>

using namespace LocLogPP;

int main(int argc, char* argv[]) {
    auto db = Database::open("./db.sqlite3");
    if (!db) {
        Logger::error("Database init failed");
        return 1;
    }
    Logger::info("Database initialized");

    auto geolocator = LocLogPP::Geolocator::create();
    if (!geolocator) {
        Logger::error("Geolocator init failed");
        return 1;
    }
    Logger::info("Geolocator initialized");

    while (true) {
        auto point = geolocator->awaitPoint();
        if (!point) {
            Logger::error("Got no point\nAssuming GPSD connection died");
            return 1;
        }

        Logger::info("Got point:\n{}", point->toString());
        db->addPoint(point.value());
    }

    return 0;
}
