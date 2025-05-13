cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)

set(SQLITEMAP_VCPKG_JSON "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json")
message(STATUS "Read contents from ${SQLITEMAP_VCPKG_JSON}")
file(READ ${SQLITEMAP_VCPKG_JSON} VCPKG_JSON_CONTENT)

message(STATUS "Extract version from ${SQLITEMAP_VCPKG_JSON}")
string(REGEX MATCH "\"version-string\"[ \t]*:[ \t]*\"([^\"]+)\"" VERSION_MATCH "${VCPKG_JSON_CONTENT}")
if (VERSION_MATCH)
    string(REGEX REPLACE "\"version-string\"[ \t]*:[ \t]*\"([^\"]+)\"" "\\1" SQLITEMAP_VERSION "${VERSION_MATCH}")
    message(STATUS "Extracted version from ${SQLITEMAP_VCPKG_JSON}: ${SQLITEMAP_VERSION}")
else()
    message(FATAL_ERROR "Could not extract version from ${SQLITEMAP_VCPKG_JSON}")
endif()

project(sqlitemap VERSION ${SQLITEMAP_VERSION} DESCRIPTION "sqlitemap is a lightweight wrapper around SQLite providing a map like interface" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

if(WIN32)
    add_definitions(-DNOMINMAX)
endif(WIN32)

find_package(unofficial-sqlite3 CONFIG REQUIRED)

option(SM_BUILD_EXAMPLES "build examples" ON)
message("SM_BUILD_EXAMPLES: ${SM_BUILD_EXAMPLES}")
if(SM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

option(SM_BUILD_TESTS "build tests" ON)
message("SM_BUILD_TESTS: ${SM_BUILD_TESTS}")
if(SM_BUILD_TESTS)
    add_subdirectory(test)
endif()

option(SM_ENABLE_COVERAGE "Enable coverage reporting" OFF)
message("SM_ENABLE_COVERAGE: ${SM_ENABLE_COVERAGE}")

add_library(sqlitemap INTERFACE)

target_link_libraries(sqlitemap INTERFACE
    unofficial::sqlite3::sqlite3
)

target_include_directories(sqlitemap INTERFACE
    $<BUILD_INTERFACE:"${CMAKE_CURRENT_SOURCE_DIR}/include}"> 
    $<INSTALL_INTERFACE:include>
)

install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_PREFIX}/include")
