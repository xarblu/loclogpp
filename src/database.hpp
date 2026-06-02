#pragma once

#include <sqlite3.h>

#include <memory>
#include <string>

namespace LocLogPP {

/**
 * SQLite wrapper class
 */
class Database {
    sqlite3 *m_database;

    explicit Database(sqlite3 *database)
        : m_database{database}
    {}

    /**
     * Run initialization/migrations
     *
     * Returns the code of the last failed execute()
     */
    int initialize();

public:
    /**
     * Open the given database, will create it if it doesn't exist
     */
    static std::unique_ptr<Database> open(std::string path);

    /**
     * Close database connection
     */
    ~Database();
    
    /**
     * No copying allowed
     */
    Database(Database &other) = delete;
    void operator=(Database &other) = delete;

    /**
     * Execute a query, error messages are printed to cerr
     */
    int execute(std::string query, int (*callback)(void*,int,char**,char**) = nullptr, void *callbackArg0 = nullptr);

    /**
     * Add a point to the DB
     */
    void addPoint(int timestamp, float latitude, float longitude, float velocity, float accuracy, float altitude);
};

} // namespace LocLogPP
