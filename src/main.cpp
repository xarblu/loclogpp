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

    Logger::attachConfig(args);

    auto db = Database::open(args->dbFile());
    if (!db) {
        Logger::error("Database init failed");
        return 1;
    }
    Logger::info("Database initialized");


    switch (args->operation()) {
        case Operation::TRACK:
            return Geolocator::track(args, db.get());

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
