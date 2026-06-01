#include "database.hpp"

#include <sqlite3.h>

#include <string>
#include <memory>
#include <iostream>

std::unique_ptr<LocLogPP::Database> LocLogPP::Database::open(std::string path) {
    sqlite3 *database;

    if (sqlite3_open(path.c_str(), &database) != SQLITE_OK) {
        return nullptr;
    }

    return std::unique_ptr<Database>{new Database(database)};
}

LocLogPP::Database::~Database() {
    if (m_database) {
        if (sqlite3_close(m_database) != SQLITE_OK) {
            std::cerr << "Closing database failed!\n";
        }
    }
}
