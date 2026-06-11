cmake_minimum_required(VERSION 4.1.0)

project(loclogpp
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(PkgConfig REQUIRED)

# sqlite3
pkg_search_module(SQLITE3 REQUIRED sqlite3)

# curl
pkg_search_module(LIBCURL REQUIRED libcurl)

# gpsd
pkg_search_module(LIBGPS REQUIRED libgps)

# default to a Release build with debug info
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build Type")
endif()

message(STATUS "---------------------------------")
message(STATUS "Summary:")
message(STATUS "")
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "Using SQLITE3: ${SQLITE3_VERSION}")
message(STATUS "Using LIBCURL: ${LIBCURL_VERSION}")
message(STATUS "Using LIBGPS:  ${LIBGPS_VERSION}")
message(STATUS "---------------------------------")

add_subdirectory(src)
add_subdirectory(meta)
