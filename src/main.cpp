#include "database.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    auto db = LocLogPP::Database::open("./db.sqlite3");
    if (!db) {
        std::cerr << "Database init failed\n";
        return 1;
    }

    return 0;
}
