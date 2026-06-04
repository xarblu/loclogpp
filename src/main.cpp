#include "database.hpp"
#include "geolocator.hpp"
#include "logger.hpp"
#include "exporter.hpp"
#include "argparser.hpp"

#include <string>
#include <string>

using namespace LocLogPP;

int main(int argc, char* argv[]) {
    auto [ret, args] = ArgParser::parse(argc, argv);
    if (!args) {
        return ret;
    }

    auto db = Database::open("./db.sqlite3");
    if (!db) {
        Logger::error("Database init failed");
        return 1;
    }
    Logger::info("Database initialized");


    switch (args->operation()) {
        case Operation::TRACK: {
            auto geolocator = LocLogPP::Geolocator::create();
            if (!geolocator) {
                Logger::error("Geolocator init failed");
                return 1;
            }
            Logger::info("Geolocator initialized");

            auto points = db->getPoints();
            Logger::info("Database contains {} points", points.size());
            if (!points.empty()) {
                Logger::info("Last point:\n{}", points.back().toString());
            }

            while (true) {
                auto point = geolocator->awaitPoint();
                if (!point) {
                    Logger::error("Got no point\nAssuming GPSD connection died");
                    return 1;
                }

                Logger::info("Got point:\n{}", point->toString());
                db->addPoint(point.value());
            }

            break;
        }

        case Operation::EXPORT: {
            std::cout << Exporter::toGPX(db.get());
            break;
        }

        [[unlikely]] default:
            Logger::error("Unknown OPERATION");
            return 1;
    }

    return 0;
}
