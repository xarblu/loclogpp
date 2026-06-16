#pragma once

#include "database.hpp"

#include <string>

namespace LocLogPP {

class Exporter {
public:
    /**
     * Export DB to GPX compatible format
     * (as exported from Dawarich https://github.com/Freika/dawarich)
     *
     * XXX: maybe just write to STDOUT instead of parsing everything into a string?
     *      for *very* large DBs this might be a memory issue
     */
    static std::string toGPX(Database *db);
};

} // namespace LocLogPP
