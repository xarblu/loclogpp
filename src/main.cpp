#include "database.hpp"
#include "geolocator.hpp"
#include "logger.hpp"
#include "exporter.hpp"
#include "argparser.hpp"

#include <memory>
#include <thread>
#include <chrono>

using namespace LocLogPP;

int main(int argc, char* argv[]) {
    auto [ret, args] = ArgParser::parse(argc, argv);
    if (!args) {
        return ret;
    }

    auto db = Database::open(args->dbFile());
    if (!db) {
        Logger::error("Database init failed");
        return 1;
    }
    Logger::info("Database initialized");


    switch (args->operation()) {
        case Operation::TRACK: {
            auto points = db->getPoints();
            Logger::info("Database contains {} points", points.size());
            if (!points.empty()) {
                Logger::info("Last point:\n{}", points.back().toString());
            }

            std::unique_ptr<Geolocator> geolocator{nullptr};
            while (!(geolocator = LocLogPP::Geolocator::create(args))) {
                Logger::error("Geolocator init failed");
                Logger::warn("Retrying in 5s");
                std::this_thread::sleep_for(std::chrono::seconds{5});
            }
            Logger::info("Geolocator initialized");

            while (true) {
                auto point = geolocator->awaitPoint();
                if (!point) {
                    Logger::error("Got no point - assuming GPSD connection died");
                    Logger::warn("Re-creating Geolocator");
                    while (!(geolocator = LocLogPP::Geolocator::create(args))) {
                        Logger::error("Geolocator init failed");
                        Logger::warn("Retrying in 5s");
                        std::this_thread::sleep_for(std::chrono::seconds{5});
                    }
                    continue;
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
