#include "database.hpp"

#include "logger.hpp"

#include <sqlite3.h>

#include <string>
#include <string_view>
#include <memory>
#include <format>
#include <vector>

std::unique_ptr<LocLogPP::Database> LocLogPP::Database::open(std::string path) {
    sqlite3 *rawDatabase;

    if (sqlite3_open(path.c_str(), &rawDatabase) != SQLITE_OK) {
        Logger::error("Opening database at {} failed!", path);
        return nullptr;
    }

    auto database = std::unique_ptr<Database>{new Database(rawDatabase)};

    if (auto ret = database->initialize(); ret > 0) {
        Logger::error("Database initialization failed!");
        return nullptr;
    }

    return database;
}

LocLogPP::Database::~Database() {
    if (m_database) {
        if (sqlite3_close(m_database) != SQLITE_OK) {
            Logger::error("Closing database failed!");
        }
    }
}

int LocLogPP::Database::initialize() {
    int ret{0};
    
    bool hasSchema{false};
    ret = execute("SELECT name FROM sqlite_master WHERE type='table' AND name='schema';", [](void* data, int valueCount, char **values, char **cols) -> int {
        *static_cast<bool*>(data) = true;
        return 0;
    },
    &hasSchema);
    if (ret > 0) return ret;

    if (!hasSchema) {
        Logger::info("Initializing new database");

        // schema metadata
        Logger::info("Creating schema table");
        ret = execute("CREATE TABLE IF NOT EXISTS schema (version INTEGER PRIMARY KEY);");
        if (ret > 0) return ret;

        ret = execute("INSERT INTO schema VALUES (0);");
        if (ret > 0) return ret;
    }

    int version{-1};
    ret = execute("SELECT version FROM schema;", [](void* data, int valueCount, char **values, char **cols) -> int {
        *static_cast<int*>(data) = std::stoi(values[0]);
        return 0;
    },
    &version);
    if (ret > 0) return ret;

    // actual schema
    if (version < 1) {
        Logger::info("Beginning migration to schema version: 1");

        ret = execute("BEGIN TRANSACTION;");
        if (ret > 0) return ret;

        /**
         * Points table
         * timestamp: unix timestamp
         * latitude: coordinate
         * longitude: coordinate
         * speed: m/s
         * accuracy: m
         * altitude: m
         */
        ret = execute("CREATE TABLE IF NOT EXISTS points (id INTEGER PRIMARY KEY, timestamp INTEGER, latitude REAL, longitude REAL, speed REAL, accuracy REAL, altitude REAL);");
        if (ret > 0) {
            execute("ROLLBACK;");
            return ret;
        }

        /**
         * Uploads table
         * id: point id from the points table
         * timestamp: timestamp of the upload
         */
        ret = execute("CREATE TABLE IF NOT EXISTS uploads (id INTEGER, timestamp INTEGER, FOREIGN KEY(id) REFERENCES points(id));");
        if (ret > 0) {
            execute("ROLLBACK;");
            return ret;
        }

        // bump version
        ret = execute("UPDATE schema SET version = 1;");
        if (ret > 0) {
            execute("ROLLBACK;");
            return ret;
        }

        ret = execute("COMMIT;");
        if (ret > 0) return ret;
    }

    return 0;
}

int LocLogPP::Database::execute(std::string query, int (*callback)(void*,int,char**,char**), void *callbackArg0) {
    char *errmsg;

    int ret = sqlite3_exec(m_database, query.c_str(), callback, callbackArg0, &errmsg);
    if (ret > 0) {
        Logger::error("SQLite3 error: {}", std::string_view{errmsg});
        sqlite3_free(errmsg);
    }

    return ret;
}

void LocLogPP::Database::addPoint(const Point &point) {
    Logger::info("Adding point to DB");

    int ret = execute(std::format("INSERT INTO points VALUES {};", point.toSQL()));

    if (ret > 0) {
        Logger::error("Adding point failed");
    } else {
        Logger::info("Successfully added point");
    }
}

std::vector<LocLogPP::Point> LocLogPP::Database::getPoints() const {
    auto statement = std::unique_ptr<sqlite3_stmt, decltype([](sqlite3_stmt *stmt) { sqlite3_finalize(stmt); })>{nullptr};

    int ret;
    std::vector<Point> points{};

    const char* query = "SELECT * FROM points;";
    ret = sqlite3_prepare_v2(m_database, query, -1, std::out_ptr(statement), NULL);
    if (ret != SQLITE_OK) {
        Logger::error("SQLite3 error while preparing \"{}\": {}", query, ret);
        return points;
    }

    uint failed{0};
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        auto point = Point::fromSQL(statement.get());
        if (!point) [[unlikely]] {
            failed++;
            continue;
        }
        points.emplace_back(std::move(point.value()));
    }

    if (failed > 0) {
        Logger::warn("Failed to parse {} points from DB", failed);
    }

    return points;
}
