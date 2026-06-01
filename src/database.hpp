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

public:
    /**
     * Open the given database, will create it if it doesn't exist
     */
    std::unique_ptr<Database> open(std::string path);

    /**
     * Close database connection
     */
    ~Database();

    
    /**
     * No copying allowed
     */
    Database(Database &other) = delete;
    void operator=(Database &other) = delete;
};

} // namespace LocLogPP
