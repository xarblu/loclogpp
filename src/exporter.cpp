#include "exporter.hpp"

#include "database.hpp"

#include <string>
#include <sstream>

std::string LocLogPP::Exporter::toGPX(Database *db) {
    std::stringstream gpx{};

    // header
    gpx << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<gpx xmlns=\"http://www.topografix.com/GPX/1/1\" version=\"1.1\" creator=\"LocLogPP\">\n"
        << "  <trk>\n"
        << "    <name>loclogpp_export.gpx</name>\n"
        << "    <trkseg>\n";

    // points
    auto points = db->getPoints();
    for (const auto &point : points) {
        gpx << point.toGPX();
    }

    // footer
    gpx << "    </trkseg>\n"
        << "  </trk>\n"
        << "</gpx>\n";

    return std::move(gpx.str());
}
