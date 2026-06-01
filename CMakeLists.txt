cmake_minimum_required(VERSION 4.1.0)

project(loclogpp
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED 23)

# Dependencies
find_package(PkgConfig REQUIRED)
pkg_search_module(SQLITE3 REQUIRED sqlite3)
pkg_search_module(LIBCURL REQUIRED libcurl)


message(STATUS "Using SQLITE3: ${SQLITE3_VERSION}")
message(STATUS "Using LIBCURL: ${LIBCURL_VERSION}")

add_subdirectory(src)
