#pragma once

#include "database.hpp"

#include <string>

namespace LocLogPP {

class Exporter {
public:
    /**
     * Export DB to GPX compatible format
     * (as exported from Dawarich https://github.com/Freika/dawarich)
     */
    static std::string toGPX(Database *db);
};

} // namespace LocLogPP
