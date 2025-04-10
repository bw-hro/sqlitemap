// sqlitemap
// SPDX-FileCopyrightText: 2024-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <catch2/catch_all.hpp>

#include <bw/tempdir/tempdir.hpp>

#include <bw/sqlitemap/sqlitemap.hpp>

using namespace bw::sqlitemap;

using namespace bw::tempdir;
namespace fs = std::filesystem;

TEST_CASE("statement check ok", "[helper]")
{
    using namespace Catch::Matchers;

    REQUIRE_NOTHROW(details::check_ok(SQLITE_OK));

    REQUIRE_THROWS_MATCHES(details::check_ok(SQLITE_ERROR), sqlitemap_error,
                           MessageMatches(StartsWith("sqlitemap_error") &&
                                          ContainsSubstring("Statement failed") &&
                                          ContainsSubstring("Expect return code 0 but was 1")));

    REQUIRE_THROWS_MATCHES(
        details::check_ok(SQLITE_ERROR, "It just failed."), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") && ContainsSubstring("It just failed")));

    // TODO check with real sqlite connection and passed in db
}

TEST_CASE("statement check done", "[helper]")
{
    using namespace Catch::Matchers;

    REQUIRE_NOTHROW(details::check_done(SQLITE_DONE));

    REQUIRE_THROWS_MATCHES(details::check_done(SQLITE_ERROR), sqlitemap_error,
                           MessageMatches(StartsWith("sqlitemap_error") &&
                                          ContainsSubstring("Statement failed") &&
                                          ContainsSubstring("Expect return code 101 but was 1")));

    REQUIRE_THROWS_MATCHES(
        details::check_done(SQLITE_ERROR, "It just failed."), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") && ContainsSubstring("It just failed")));

    // TODO check with real sqlite connection and passed in db
}

TEST_CASE("sqlitemap helps with sql statement creation by providing table name")
{
    // test for default table name "unnamed"
    sqlitemap sm_unnamed;
    REQUIRE(sm_unnamed.sql("select * from :table") == R"(select * from "unnamed")");

    // test for custom table name
    TempDir temp_dir(Config().enable_logging());
    auto file = (temp_dir.path() / "db.sqlite").string();

    sqlitemap sm_named(file, "projects");
    REQUIRE(sm_named.sql("select * from :table") == R"(select * from "projects")");

    sqlitemap sm_named_advanced(file, R"(Näme '42')");
    REQUIRE(sm_named_advanced.sql("select * from :table") == R"(select * from "Näme '42'")");

    sqlitemap sm_named_sic(file, ":table");
    REQUIRE(sm_named_sic.sql("select * from :table") == R"(select * from ":table")");
}

TEST_CASE("sqlitemap can query table names from file")
{
    REQUIRE_THROWS_AS(bw::sqlitemap::get_tablenames("some-not-existing.sqlite"), sqlitemap_error);

    TempDir temp_dir(Config().enable_logging());
    auto file = (temp_dir.path() / "db.sqlite").string();

    sqlitemap sm_unnamed(file);
    sqlitemap sm_named(file, "projects");
    sqlitemap sm_named_advanced(file, "Näme '42'");
    sqlitemap sm_named_sic(file, ":table");

    auto tables = bw::sqlitemap::get_tablenames(file);

    std::cout << std::endl << "Found tables: " << tables.size() << std::endl;
    for (auto table : tables)
    {
        std::cout << "- " << table << std::endl;
    }
    std::cout << std::endl;

    REQUIRE(std::find(tables.begin(), tables.end(), "unnamed") != tables.end());
    REQUIRE(std::find(tables.begin(), tables.end(), "projects") != tables.end());
    REQUIRE(std::find(tables.begin(), tables.end(), "Näme '42'") != tables.end());
    REQUIRE(std::find(tables.begin(), tables.end(), ":table") != tables.end());
}

TEST_CASE("sqlitemap can be configured to use custom logger")
{
    std::string log_content;

    auto test_logger = [&](log_level level, const std::string& msg)
    {
        if (level > log_level::info)
            log_content = "ERROR:" + msg;
        else
            log_content = "INFO:" + msg;

        std::cout << "level:" << (int)level << " content:" << log_content << std::endl;
    };

    sqlitemap sm(config().log_level(log_level::debug).log_impl(test_logger));

    sm.log().error("db broken...");
    REQUIRE(log_content == "ERROR:db broken...");

    sm.log().info("db connected...");
    REQUIRE(log_content == "INFO:db connected...");

    sm.log().debug("db running...");
    REQUIRE(log_content == "INFO:db running...");

    log_content.clear();
    sm.log().trace("db idle...");
    REQUIRE(log_content.empty());
}
