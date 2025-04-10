// sqlitemap
// SPDX-FileCopyrightText: 2024-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <bw/sqlitemap/sqlitemap.hpp>
#include <catch2/catch_all.hpp>
#include <thread>

#include <bw/tempdir/tempdir.hpp>

using namespace bw::sqlitemap;
using namespace bw::tempdir;
namespace fs = std::filesystem;

TEST_CASE("sqlitemap assignment")
{
    // as variable default codec
    sqlitemap sm;
    REQUIRE(sm.empty());

    // as variable with custom codecs
    sqlitemap smc(config<int, double>());

    // as unique_ptr
    auto sm_ptr = std::make_unique<sqlitemap<>>();

    // as unique_ptr with custom codecs
    using codec_pair = decltype(config<int, double>().codecs());
    auto smc_ptr = std::make_unique<sqlitemap<codec_pair>>(config<int, double>());

    // as member of a class
    class App
    {
        using DB = sqlitemap<>;

      public:
        App(const std::string& file)
            : db(file, "cache")
        {
            db.set("app-1", "123");
        }

      private:
        DB db;
    };

    TempDir temp_dir(Config().enable_logging());
    auto file = temp_dir.path() / "db.sqlite";

    REQUIRE_NOTHROW(App(file.string()));
    REQUIRE((sqlitemap(file.string(), "cache", operation_mode::r).get("app-1") == "123"));
}

TEST_CASE("sqlitemap can be represented as string")
{
    sqlitemap sm(config().filename(":memory:"));
    REQUIRE(sm.to_string() == "sqlitemap(:memory:)");

    sm.close();
    REQUIRE(sm.to_string() == "sqlitemap(:memory:)");
}

TEST_CASE("Uses temporary file when no filename is provided")
{
    sqlitemap sm;
    REQUIRE(sm.in_temp());
    REQUIRE(sm.config().filename() != "");
    REQUIRE(fs::exists(sm.config().filename()));

    sm.close();
    REQUIRE_FALSE(fs::exists(sm.config().filename()));
}

TEST_CASE("Optionally can operate in memory only")
{
    sqlitemap sm(config().filename(":memory:"));
    REQUIRE(sm.in_memory());
    REQUIRE(sm.config().filename() == ":memory:");
    REQUIRE_FALSE(fs::exists(sm.config().filename()));
}

TEST_CASE("Checks if database containing directory exists")
{
    auto file = "./not-existing-dir/db.sqlite";
    REQUIRE_FALSE(fs::exists(file));

    using namespace Catch::Matchers;
    REQUIRE_THROWS_MATCHES(sqlitemap(config().filename(file)), sqlitemap_error,
                           MessageMatches(StartsWith("sqlitemap_error") &&
                                          ContainsSubstring("directory does not exist")));
}

TEST_CASE("Supports relative path of database file")
{
    auto file1 = "db.sqlite";
    REQUIRE_FALSE(fs::exists(file1));
    REQUIRE_NOTHROW(sqlitemap(config().filename(file1)));

    auto file2 = "./db.sqlite";
    REQUIRE(fs::exists(file2));
    REQUIRE_NOTHROW(sqlitemap(config().filename(file2)));

    fs::remove(file2);
    REQUIRE_FALSE(fs::exists(file1));
    REQUIRE_FALSE(fs::exists(file2));
}

TEST_CASE("Supports recreation on each instantiation")
{
    TempDir temp_dir(Config().enable_logging());
    auto file = (temp_dir.path() / "db.sqlite").string();

    // first instance
    {
        sqlitemap sm_1(config().filename(file).mode(operation_mode::n));
        REQUIRE(sm_1.empty());

        sm_1.set("key_1", "val_1");
        sm_1.commit();

        REQUIRE(sm_1.size() == 1);
        REQUIRE((sm_1["key_1"] == "val_1"));
    }

    // second instance

    sqlitemap sm_2(config().filename(file).mode(operation_mode::n));

    REQUIRE(sm_2.empty());

    sm_2.set("key_2", "val_2");
    sm_2.commit();

    REQUIRE(sm_2.size() == 1);
    REQUIRE((sm_2["key_1"] == ""));
    REQUIRE((sm_2["key_2"] == "val_2"));
}

TEST_CASE("Size of sqlitemap can be queryied")
{
    sqlitemap sm;
    REQUIRE(sm.empty());
    REQUIRE(sm.size() == 0);

    sm.set("k1", "v1");
    REQUIRE_FALSE(sm.empty());
    REQUIRE(sm.size() == 1);

    sm.set("k2", "v2");
    sm.set("k3", "v3");
    sm.set("k4", "v4");
    sm.set("k5", "v5");
    sm.set("k6", "v6");
    sm.set("k7", "v7");
    sm.set("k8", "v8");
    sm.set("k9", "v9");
    REQUIRE(sm.size() == 9);

    sm["k10"] = "v10";
    sm["k11"] = "v11";
    sm["k12"] = "v12";
    REQUIRE(sm.size() == 12);

    for (int i = sm.size(); i > 0; i--)
        sm.del("k" + std::to_string(i));
    REQUIRE(sm.empty());
    REQUIRE(sm.size() == 0);
}

TEST_CASE("Create sqlitemap")
{
    static TempDir temp_dir(Config().enable_logging());
    static auto given_file = temp_dir.path() / "db.sqlite";

    std::cout << std::endl << "static given_file:'" << given_file << "'" << std::endl;

    auto mode = GENERATE(operation_mode::c,  //
                         operation_mode::w,  //
                         operation_mode::n); //

    auto file = GENERATE_COPY("", ":memory:", given_file.c_str());

    std::cout << std::endl << "file:'" << file << "' mode:" << (int)mode << std::endl;

    sqlitemap sm(config().filename(file).mode(mode));

    std::cout << "sqltiemap db:'" << sm.config().filename() << "'" << std::endl;

    REQUIRE(sm.empty());
    REQUIRE(sm.size() == 0);
}

TEST_CASE("Terminate sqlitemap")
{
    using namespace Catch::Matchers;

    static TempDir temp_dir(Config().enable_logging());
    static auto given_file = temp_dir.path() / "db.sqlite";

    std::cout << std::endl << "static given_file:'" << given_file << "'" << std::endl;

    operation_mode mode = GENERATE(operation_mode::c,  //
                                   operation_mode::w,  //
                                   operation_mode::n); //

    std::string file_gen = GENERATE_COPY("", ":memory:", given_file.c_str());

    std::string file = file_gen;
    if (file_gen != "" && file_gen != ":memory:")
    {
        file += "_m" + std::to_string((int)mode);

        // first ensure that fresh database exists
        {
            sqlitemap create(config().filename(file).mode(operation_mode::n));
            REQUIRE(create.empty());
        }

        REQUIRE(std::filesystem::exists(file));
    }

    std::cout << std::endl << "file:'" << file << "' mode:" << (int)mode << std::endl;
    sqlitemap sm(config().filename(file).mode(mode));
    std::cout << "sqltiemap db:'" << sm.config().filename() << "'" << std::endl;

    // test termination

    sm.set("key", "value");
    REQUIRE((sm.get("key") == "value"));

    REQUIRE_NOTHROW(sm.terminate());
    REQUIRE_FALSE(std::filesystem::exists(file));
}

TEST_CASE("Read-only mode 'r' does not allow sqlitemap db creation/termination")
{
    using namespace Catch::Matchers;

    static TempDir temp_dir(Config().enable_logging());
    static auto file = (temp_dir.path() / "db.sqlite").string();

    REQUIRE_FALSE(std::filesystem::exists(file));

    { // check no db will be created when file does not exists
        REQUIRE_THROWS_MATCHES(sqlitemap(config().filename("").mode(operation_mode::r)),
                               sqlitemap_error,
                               MessageMatches(StartsWith("sqlitemap_error") &&
                                              ContainsSubstring("Cannot open database")));

        REQUIRE_THROWS_MATCHES(sqlitemap(config().filename(":memory:").mode(operation_mode::r)),
                               sqlitemap_error,
                               MessageMatches(StartsWith("sqlitemap_error") &&
                                              ContainsSubstring("File :memory: does not exist")));

        REQUIRE_THROWS_MATCHES(sqlitemap(config().filename(file).mode(operation_mode::r)),
                               sqlitemap_error,
                               MessageMatches(StartsWith("sqlitemap_error") &&
                                              ContainsSubstring("Cannot open database")));
    }

    REQUIRE_FALSE(std::filesystem::exists(file));

    { // ensure db exists with contents
        sqlitemap sm(config().filename(file).table("table_a").mode(operation_mode::c));
        sm.set("k1", "v1");
    }

    REQUIRE(std::filesystem::exists(file));

    { // check no db will be created when table does not exists
        REQUIRE_THROWS_MATCHES(
            sqlitemap(config().filename(file).table("table_x").mode(operation_mode::r)),
            sqlitemap_error,
            MessageMatches(StartsWith("sqlitemap_error") &&
                           ContainsSubstring("Refusing to create a new table") &&
                           ContainsSubstring("in read-only DB mode")));
    }

    // check values can be read from existing db table
    sqlitemap sm(config().filename(file).table("table_a").mode(operation_mode::r));
    REQUIRE_FALSE(sm.empty());
    REQUIRE(sm.size() == 1);
    REQUIRE((sm["k1"] == "v1"));

    // check termination is not allowed
    REQUIRE_THROWS_MATCHES(
        sm.terminate(), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to terminate read-only sqlitemap")));
}

TEST_CASE("Read-only mode 'r' does not allow to change sqlitemap")
{
    using namespace Catch::Matchers;

    static TempDir temp_dir(Config().enable_logging());
    static auto file = (temp_dir.path() / "db.sqlite").string();

    { // ensure db exists with contents
        sqlitemap sm(config().filename(file).table("table_a").mode(operation_mode::c));
        sm.set("k1", "v1");
    }

    REQUIRE(std::filesystem::exists(file));

    // check values can be read from existing db table
    sqlitemap sm(config().filename(file).table("table_a").mode(operation_mode::r));
    REQUIRE_FALSE(sm.empty());
    REQUIRE(sm.size() == 1);
    REQUIRE((sm["k1"] == "v1"));

    // check no new keys can be added
    REQUIRE_THROWS_MATCHES(
        sm.set("k2", "v2"), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to write to read-only sqlitemap")));

    // check no new keys can be inserted
    REQUIRE_THROWS_MATCHES(
        sm.insert(std::pair{"k2", "v2"}), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to insert into read-only sqlitemap")));

    REQUIRE_THROWS_MATCHES(
        sm.insert({{"k2", "v2"}, {"k3", "v3"}}), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to insert into read-only sqlitemap")));

    std::vector<std::pair<std::string, std::string>> key_value_pairs{{"k2", "v2"}, {"k3", "v3"}};
    REQUIRE_THROWS_MATCHES(
        sm.insert(key_value_pairs.begin(), key_value_pairs.end()), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to insert into read-only sqlitemap")));

    REQUIRE_THROWS_MATCHES(
        sm.insert_or_assign("k2", "v2"), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to write to read-only sqlitemap")));

    std::string k2 = "k2";
    REQUIRE_THROWS_MATCHES(
        sm.insert_or_assign(k2, "v2"), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to write to read-only sqlitemap")));

    // check no key can be emplaced
    REQUIRE_THROWS_MATCHES(
        sm.emplace("k2", "v2"), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to write to read-only sqlitemap")));

    REQUIRE_THROWS_MATCHES(
        sm.emplace_hint(sm.cbegin(), "k2", "v2"), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to write to read-only sqlitemap")));

    REQUIRE_THROWS_MATCHES(
        sm.try_emplace("k2", "v2"), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to write to read-only sqlitemap")));

    REQUIRE_THROWS_MATCHES(
        sm.try_emplace(k2, "v2"), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to write to read-only sqlitemap")));

    REQUIRE_THROWS_MATCHES(
        sm.try_emplace(sm.cbegin(), "k2", "v2"), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to write to read-only sqlitemap")));

    REQUIRE_THROWS_MATCHES(
        sm.try_emplace(sm.cend(), k2, "v2"), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to write to read-only sqlitemap")));

    // check no keys can be extracted
    REQUIRE_THROWS_MATCHES(
        sm.extract("k1"), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to extract from read-only sqlitemap")));

    REQUIRE_THROWS_MATCHES(
        sm.extract(sm.cbegin()), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to extract from read-only sqlitemap")));

    // check no keys can be erased
    REQUIRE_THROWS_MATCHES(
        sm.erase("k1"), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to erase from read-only sqlitemap")));

    REQUIRE_THROWS_MATCHES(
        sm.erase_if([](auto entry) { return true; }), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to erase from read-only sqlitemap")));

    // check no keys can be deleted
    REQUIRE_THROWS_MATCHES(
        sm.del("k1"), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to delete from read-only sqlitemap")));

    // check table can not be cleared
    REQUIRE_THROWS_MATCHES(
        sm.clear(), sqlitemap_error,
        MessageMatches(StartsWith("sqlitemap_error") &&
                       ContainsSubstring("Refusing to clear read-only sqlitemap")));
}

TEST_CASE("Write mode 'w' drops table contents on initialization")
{
    TempDir temp_dir(Config().enable_logging());
    auto file = (temp_dir.path() / "db.sqlite").string();

    { // create db with content
        sqlitemap sm(config().filename(file).mode(operation_mode::c));
        sm.set("k1", "v1");
        REQUIRE((sm["k1"] == "v1"));
    }

    { // check content is still present
        sqlitemap sm(config().filename(file).mode(operation_mode::r));
        REQUIRE((sm["k1"] == "v1"));
    }

    // check content will be dropped
    sqlitemap sm(config().filename(file).mode(operation_mode::w));
    REQUIRE(sm.empty());
    REQUIRE(sm.size() == 0);
    REQUIRE((sm["k1"] == ""));
}

TEST_CASE("Assign values operator[]")
{
    TempDir temp_dir;
    auto file = (temp_dir.path() / "db.sqlite").string();

    sqlitemap sm(config().filename(file));
    sm["foo"] = "bar";

    REQUIRE(sm.size() == 1);
    REQUIRE((sm["foo"] == "bar"));

    std::string val = sm["foo"];
    REQUIRE(val == "bar");
}

TEST_CASE("Assign values at()")
{
    TempDir temp_dir;
    auto file = (temp_dir.path() / "db.sqlite").string();

    sqlitemap sm(config().filename(file));
    sm["foo"] = "bar";
    REQUIRE(sm.size() == 1);
    REQUIRE((sm["foo"] == "bar"));

    auto foo = sm.at("foo");
    std::string val = foo;
    REQUIRE(val == "bar");

    foo = "baz";

    REQUIRE(sm.get("foo") == "baz");
}

TEST_CASE("Insert data")
{
    TempDir temp_dir;
    auto file = (temp_dir.path() / "db.sqlite").string();

    sqlitemap sm(config().filename(file));

    sm.insert({{"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}, {"k4", "v4"}, {"k5", "v5"}});
    REQUIRE(sm.size() == 5);
    REQUIRE((sm["k1"] == "v1"));
    REQUIRE((sm["k2"] == "v2"));
    REQUIRE((sm["k3"] == "v3"));
    REQUIRE((sm["k4"] == "v4"));
    REQUIRE((sm["k5"] == "v5"));

    std::map<std::string, std::string> values;
    values["m1"] = "v1";
    values["m2"] = "v2";
    values["m3"] = "v3";
    values["m4"] = "v4";
    values["m5"] = "v5";
    auto start = std::next(values.cbegin());
    auto end = std::prev(values.cend());

    sm.insert(start, end);
    REQUIRE(sm.size() == 8);
    REQUIRE((sm["m2"] == "v2"));
    REQUIRE((sm["m3"] == "v3"));
    REQUIRE((sm["m4"] == "v4"));

    const std::pair new_entry{"s1", "v1"};
    auto result = sm.insert(new_entry);
    REQUIRE(sm.size() == 9);
    REQUIRE(result.second);
    REQUIRE(result.first->first == "s1");
    REQUIRE(result.first->second == "v1");

    result = sm.insert(std::pair{"s1", "v2"});
    REQUIRE(sm.size() == 9);
    REQUIRE_FALSE(result.second);
    REQUIRE(result.first->first == "s1");
    REQUIRE(result.first->second == "v1");

    sm.insert({{"k1", "u1"}, {"m1", "u1"}, {"s1", "u1"}});
    REQUIRE(sm.size() == 10);
    REQUIRE((sm["k1"] == "v1"));
    REQUIRE((sm["m1"] == "u1"));
    REQUIRE((sm["s1"] == "v1"));
}

TEST_CASE("Insert or assign data")
{
    TempDir temp_dir;
    auto file = (temp_dir.path() / "db.sqlite").string();

    sqlitemap sm(config().filename(file));

    auto result = sm.insert_or_assign("k1", "v1");
    REQUIRE(sm.size() == 1);
    REQUIRE(result.second);
    REQUIRE(result.first->first == "k1");
    REQUIRE(result.first->second == "v1");
    REQUIRE((sm["k1"] == "v1"));

    std::string k2 = "k2";
    result = sm.insert_or_assign(k2, "v2");
    REQUIRE(sm.size() == 2);
    REQUIRE(result.second);
    REQUIRE(result.first->first == "k2");
    REQUIRE(result.first->second == "v2");
    REQUIRE((sm["k2"] == "v2"));

    result = sm.insert_or_assign("k1", "u1");
    REQUIRE(sm.size() == 2);
    REQUIRE_FALSE(result.second);
    REQUIRE(result.first->first == "k1");
    REQUIRE(result.first->second == "u1");
    REQUIRE((sm["k1"] == "u1"));

    result = sm.insert_or_assign(k2, "u2");
    REQUIRE(sm.size() == 2);
    REQUIRE_FALSE(result.second);
    REQUIRE(result.first->first == "k2");
    REQUIRE(result.first->second == "u2");
    REQUIRE((sm["k2"] == "u2"));
}

TEST_CASE("Emplace data")
{
    TempDir temp_dir;
    auto file = (temp_dir.path() / "db.sqlite").string();

    sqlitemap sm(config().filename(file));
    auto result = sm.emplace("k1", "v1");
    REQUIRE(sm.size() == 1);
    REQUIRE(result.second);
    REQUIRE(result.first->first == "k1");
    REQUIRE(result.first->second == "v1");
    REQUIRE((sm["k1"] == "v1"));

    result = sm.emplace(std::make_pair("k2", "v2"));
    REQUIRE(sm.size() == 2);
    REQUIRE(result.second);
    REQUIRE(result.first->first == "k2");
    REQUIRE(result.first->second == "v2");
    REQUIRE((sm["k2"] == "v2"));

    result = sm.emplace(std::piecewise_construct,    //
                        std::forward_as_tuple("k3"), //
                        std::forward_as_tuple(3, 'v'));
    REQUIRE(sm.size() == 3);
    REQUIRE(result.second);
    REQUIRE(result.first->first == "k3");
    REQUIRE(result.first->second == "vvv");
    REQUIRE((sm["k3"] == "vvv"));

    result = sm.emplace("k3", "x");
    REQUIRE(sm.size() == 3);
    REQUIRE_FALSE(result.second);
    REQUIRE(result.first->first == "k3");
    REQUIRE(result.first->second == "vvv");
    REQUIRE((sm["k3"] == "vvv"));
}

TEST_CASE("Try emplace data")
{
    TempDir temp_dir;
    auto file = (temp_dir.path() / "db.sqlite").string();

    sqlitemap sm(config().filename(file));
    auto result = sm.try_emplace("k1", "v1");
    REQUIRE(sm.size() == 1);
    REQUIRE(result.second);
    REQUIRE(result.first->first == "k1");
    REQUIRE(result.first->second == "v1");
    REQUIRE((sm["k1"] == "v1"));

    std::string k2 = "k2";
    result = sm.try_emplace(k2, "v2");
    REQUIRE(sm.size() == 2);
    REQUIRE(result.second);
    REQUIRE(result.first->first == "k2");
    REQUIRE(result.first->second == "v2");
    REQUIRE((sm["k2"] == "v2"));

    result = sm.try_emplace("k3", 3, 'v');
    REQUIRE(sm.size() == 3);
    REQUIRE(result.second);
    REQUIRE(result.first->first == "k3");
    REQUIRE(result.first->second == "vvv");
    REQUIRE((sm["k3"] == "vvv"));

    result = sm.try_emplace("k3", "x");
    REQUIRE(sm.size() == 3);
    REQUIRE_FALSE(result.second);
    REQUIRE(result.first->first == "k3");
    REQUIRE(result.first->second == "vvv");
    REQUIRE((sm["k3"] == "vvv"));
}

TEST_CASE("Emplace hint data")
{
    TempDir temp_dir;
    auto file = (temp_dir.path() / "db.sqlite").string();

    sqlitemap sm(config().filename(file).auto_commit(true));

    auto result = sm.emplace_hint(sm.cbegin(), "k1", "v1");
    REQUIRE(sm.size() == 1);
    REQUIRE(result != sm.end());
    REQUIRE(result->first == "k1");
    REQUIRE(result->second == "v1");
    REQUIRE((sm["k1"] == "v1"));

    result = sm.emplace_hint(sm.cend(), std::make_pair("k2", "v2"));
    REQUIRE(sm.size() == 2);
    REQUIRE(result != sm.end());
    REQUIRE(result->first == "k2");
    REQUIRE(result->second == "v2");
    REQUIRE((sm["k2"] == "v2"));

    result = sm.emplace_hint(sm.cbegin() + 1,             //
                             std::piecewise_construct,    //
                             std::forward_as_tuple("k3"), //
                             std::forward_as_tuple(3, 'v'));
    REQUIRE(sm.size() == 3);
    REQUIRE(result != sm.end());
    REQUIRE(result->first == "k3");
    REQUIRE(result->second == "vvv");
    REQUIRE((sm["k3"] == "vvv"));

    result = sm.emplace_hint(sm.cbegin(), "k3", "x");
    REQUIRE(sm.size() == 3);
    // REQUIRE(result == std::next(sm.begin(), 2)); // TODO: improve iterator equals check
    REQUIRE(result->first == "k3");
    REQUIRE(result->second == "vvv");
    REQUIRE((sm["k3"] == "vvv"));
}

TEST_CASE("Try emplace hint data")
{
    TempDir temp_dir;
    auto file = (temp_dir.path() / "db.sqlite").string();

    sqlitemap sm(config().filename(file).auto_commit(true));

    auto result = sm.try_emplace(sm.cbegin(), "k1", "v1");
    REQUIRE(sm.size() == 1);
    REQUIRE(result != sm.end());
    REQUIRE(result->first == "k1");
    REQUIRE(result->second == "v1");
    REQUIRE((sm["k1"] == "v1"));

    std::string k1 = "k1";
    result = sm.try_emplace(sm.cbegin(), k1, "x"); // emplace does not change existing keys
    REQUIRE(sm.size() == 1);
    REQUIRE(result != sm.end());
    REQUIRE(result->first == "k1");
    REQUIRE(result->second == "v1");
    REQUIRE((sm["k1"] == "v1"));

    std::string k2 = "k2";
    result = sm.try_emplace(sm.cend(), k2, "v2");
    REQUIRE(sm.size() == 2);
    REQUIRE(result != sm.end());
    REQUIRE(result->first == "k2");
    REQUIRE(result->second == "v2");
    REQUIRE((sm["k2"] == "v2"));

    result = sm.try_emplace(sm.cbegin() + 1, "k3", 3, 'v');
    REQUIRE(sm.size() == 3);
    REQUIRE(result != sm.end());
    REQUIRE(result->first == "k3");
    REQUIRE(result->second == "vvv");
    REQUIRE((sm["k3"] == "vvv"));

    result = sm.try_emplace(sm.cbegin(), "k3", "x");
    REQUIRE(sm.size() == 3);
    // REQUIRE(result == std::next(sm.begin(), 2)); // TODO: improve iterator equals check
    REQUIRE(result->first == "k3");
    REQUIRE(result->second == "vvv");
    REQUIRE((sm["k3"] == "vvv"));
}

TEST_CASE("Clear data")
{
    sqlitemap sm;

    sm.insert({{"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}});
    REQUIRE(sm.size() == 3);
    REQUIRE((sm["k1"] == "v1"));
    REQUIRE((sm["k2"] == "v2"));
    REQUIRE((sm["k3"] == "v3"));

    sm.clear();
    REQUIRE(sm.size() == 0);
    REQUIRE(sm.empty());
}

TEST_CASE("Manage one record")
{
    auto long_str =
        "Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor "
        "invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et "
        "accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata "
        "sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing "
        "elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, "
        "sed diam voluptua. At vero eos et accusam et justo duo dolores et e";

    sqlitemap sm;

    sm["the-record"] = long_str;
    REQUIRE((sm["the-record"] == long_str));
    REQUIRE(sm.size() == 1);

    sm["the-record"] = "short_str";
    REQUIRE((sm["the-record"] == "short_str"));
    REQUIRE(sm.size() == 1);

    sm.del("the-record");
    REQUIRE(sm.size() == 0);
    REQUIRE(sm.empty());
}

TEST_CASE("Manage multiple records")
{
    sqlitemap sm;

    sm["k1"] = "v1";
    sm["k2"] = "v2";
    REQUIRE(sm.size() == 2);

    std::map<std::string, std::string> entries;
    std::map<std::string, std::string> expected_entries{{"k1", "v1"}, {"k2", "v2"}};
    for (auto it = sm.begin(); it != sm.end(); ++it)
    {
        entries.emplace(it->first, it->second);
    }
    REQUIRE(entries == expected_entries);
}

TEST_CASE("Get")
{
    sqlitemap sm;

    // get throws when key is missing
    REQUIRE_THROWS_AS(sm.get("k1"), sqlitemap_error);

    // try_get returns std::nullopt when key is missing
    REQUIRE_FALSE(sm.try_get("k1"));

    // at thrwos when key is missing
    REQUIRE_THROWS_AS(sm.at("k1"), sqlitemap_error);

    // operator[] store and return default constructed value when key is missing, like std::map does
    REQUIRE((sm["k1"] == std::string()));
    REQUIRE(sm.get("k1") == "");
}

TEST_CASE("Find entries")
{
    sqlitemap sm;
    REQUIRE(sm.find("k1") == sm.end());

    sm["k0"] = "v0";
    sm["k1"] = "v1";
    sm["k2"] = "v2";
    sm["k3"] = "v3";
    sm["k4"] = "v4";
    auto kv1 = sm.find("k1");

    REQUIRE(kv1 != sm.end());
    REQUIRE(kv1->first == "k1");
    REQUIRE(kv1->second == "v1");

    const auto& csm = sm;
    const auto& kv3 = csm.find("k3");
    REQUIRE(kv3->second == "v3");
}

TEST_CASE("Equal range")
{
    sqlitemap sm;
    REQUIRE(sm.find("k1") == sm.end());

    sm["k0"] = "v0";
    sm["k1"] = "v1";
    sm["k2"] = "v2";
    sm["k3"] = "v3";
    sm["k4"] = "v4";
    auto [from, to] = sm.equal_range("k1");

    REQUIRE(from != sm.end());
    REQUIRE(from->first == "k1");
    REQUIRE(from->second == "v1");

    REQUIRE(to != sm.end());
    REQUIRE(to->first == "k1");
    REQUIRE(to->second == "v1");

    auto nf = sm.equal_range("not-existing-key");
    REQUIRE(nf.first == sm.end());
    REQUIRE(nf.second == sm.end());

    const auto& csm = sm;
    const auto& [cfrom, cto] = csm.equal_range("k3");

    REQUIRE(cfrom != csm.end());
    REQUIRE(cfrom->first == "k3");
    REQUIRE(cfrom->second == "v3");

    REQUIRE(cto != csm.end());
    REQUIRE(cto->first == "k3");
    REQUIRE(cto->second == "v3");

    auto cnf = csm.equal_range("not-existing-key");
    REQUIRE(cnf.first == csm.end());
    REQUIRE(cnf.second == csm.end());
}

TEST_CASE("count entries")
{
    sqlitemap sm;
    REQUIRE_FALSE(sm.count("k1"));
    REQUIRE(sm.count("k1") == 0);

    sm["k1"] = "v1";
    REQUIRE(sm.count("k1"));
    REQUIRE(sm.count("k1") == 1);

    sm["k2"] = "v2";
    sm["k3"] = "v3";
    sm["k4"] = "v4";

    REQUIRE(sm.count("k2"));
    REQUIRE(sm.count("k3"));
    REQUIRE(sm.count("k4"));
}

TEST_CASE("contains key")
{
    sqlitemap sm;
    REQUIRE_FALSE(sm.contains("k1"));

    sm["k1"] = "v1";
    REQUIRE(sm.contains("k1"));

    sm["k2"] = "v2";
    sm["k3"] = "v3";
    sm["k4"] = "v4";

    REQUIRE(sm.contains("k2"));
    REQUIRE(sm.contains("k3"));
    REQUIRE(sm.contains("k4"));
}

TEST_CASE("erase key")
{
    sqlitemap sm;
    REQUIRE(sm.erase("k1") == 0);

    sm["k1"] = "v1";
    REQUIRE(sm.erase("k1") == 1);
    REQUIRE_FALSE(sm.contains("k1"));

    sm["k2"] = "v2";
    sm["k3"] = "v3";
    sm["k4"] = "v4";

    REQUIRE(sm.erase("k2") == 1);
    REQUIRE(sm.erase("k3") == 1);
    REQUIRE(sm.erase("k4") == 1);

    REQUIRE_FALSE(sm.contains("k2"));
    REQUIRE_FALSE(sm.contains("k3"));
    REQUIRE_FALSE(sm.contains("k4"));
}

TEST_CASE("erase key if")
{
    auto predicate = [](const auto& entry) { return entry.first[0] == 'k'; };

    sqlitemap sm;
    REQUIRE(sm.erase_if(predicate) == 0);

    sm["k1"] = "v1";
    REQUIRE(sm.erase_if(predicate) == 1);
    REQUIRE_FALSE(sm.contains("k1"));

    sm["q1"] = "v0";
    sm["k2"] = "v2";
    sm["k3"] = "v3";
    sm["q2"] = "v0";
    sm["k4"] = "v4";

    REQUIRE(sm.erase_if(predicate) == 3);
    REQUIRE_FALSE(sm.contains("k2"));
    REQUIRE_FALSE(sm.contains("k3"));
    REQUIRE_FALSE(sm.contains("k4"));

    REQUIRE(sm.contains("q1"));
    REQUIRE(sm.contains("q2"));
    REQUIRE(sm.size() == 2);
}

TEST_CASE("extract key")
{
    sqlitemap sm;
    REQUIRE_FALSE(sm.extract("k1"));

    sm["k1"] = "v1";
    sm["k2"] = "v2";
    sm["k3"] = "v3";
    sm["k4"] = "v4";

    auto extracted = sm.extract("k1");
    REQUIRE(extracted);
    REQUIRE_FALSE(sm.contains("k1"));
    REQUIRE(sm.size() == 3);

    extracted.key() = "u1";
    auto result = sm.insert(std::move(extracted));
    REQUIRE(result.inserted);
    REQUIRE_FALSE(result.node);
    REQUIRE(result.position->first == "u1");
    REQUIRE(result.position->second == "v1");

    REQUIRE((sm["k2"] == "v2"));
    REQUIRE((sm["k3"] == "v3"));
    REQUIRE((sm["k4"] == "v4"));
    REQUIRE((sm["u1"] == "v1"));
    REQUIRE(sm.size() == 4);

    result = sm.insert(std::move(result.node));
    REQUIRE_FALSE(result.node);
    REQUIRE_FALSE(result.inserted);
    REQUIRE(result.position == sm.end());

    auto pos = sm.cbegin() + 2;
    extracted = sm.extract(pos);
    REQUIRE(extracted);
    REQUIRE_FALSE(sm.contains("k4")); // keys have insertions order
    REQUIRE(sm.size() == 3);

    extracted.key() = "u4";
    extracted.mapped() = "w4";
    sm.insert(std::move(extracted));

    REQUIRE((sm["k2"] == "v2"));
    REQUIRE((sm["k3"] == "v3"));
    REQUIRE((sm["u1"] == "v1"));
    REQUIRE((sm["u4"] == "w4"));
    REQUIRE(sm.size() == 4);

    extracted = sm.extract("u4");
    extracted.key() = "k3"; // key already exists
    result = sm.insert(std::move(extracted));
    REQUIRE_FALSE(result.inserted);
    REQUIRE(result.node);
    REQUIRE(result.node.key() == "k3");
    REQUIRE(result.node.mapped() == "w4");
    REQUIRE(result.position->first == "k3");
    REQUIRE(result.position->second == "v3");

    REQUIRE_FALSE(sm.extract(sm.cend()));
}

TEST_CASE("Offers an input iterator")
{
    sqlitemap sm;

    for (int i = 0; i < 10; i++)
        sm["k" + std::to_string(i)] = "v" + std::to_string(i);

    REQUIRE(sm.size() == 10);

    std::vector<std::string> values;
    for (auto [k, v] : sm)
    {
        std::cout << k << " = " << v << std::endl;
        values.push_back(v);
    }
    std::vector<std::string> expected_values{"v0", "v1", "v2", "v3", "v4",
                                             "v5", "v6", "v7", "v8", "v9"};
    REQUIRE(values == expected_values);

    REQUIRE(*sm.begin() == std::pair<std::string, std::string>{"k0", "v0"});
    REQUIRE(std::next(sm.begin())->first == "k1");
    REQUIRE(std::next(sm.begin())->second == "v1");

    std::vector<std::string> keys;
    auto current = std::next(sm.begin(), 7);

    for (; current != sm.end(); ++current)
    {
        keys.push_back(current->first);
    }
    REQUIRE(keys == std::vector<std::string>{"k7", "k8", "k9"});
}

TEST_CASE("Iterator equals check is limited to end()", "[limitation]")
{
    sqlitemap sm;

    for (int i = 0; i < 10; i++)
        sm["k" + std::to_string(i)] = "v" + std::to_string(i);
    sm.commit();

    sqlitemap other_sm;

    REQUIRE(sm.size() == 10);

    auto it_a = sm.begin();
    auto it_b = sm.begin() + 2;
    auto it_c = sm.begin() + 2;

    // as expected
    REQUIRE(sm.end() == sm.end());
    REQUIRE(sm.end() != it_a);
    REQUIRE(sm.end() != it_b);

    REQUIRE(sm.begin() != other_sm.begin());
    REQUIRE(sm.begin() + 1 != sm.begin() + 3);

    // limitation, would be better if results would be equal
    REQUIRE(it_a != sm.begin());
    REQUIRE(it_a != it_b);
    REQUIRE(it_b != it_c);
    REQUIRE(sm.begin() != sm.begin());
}

TEST_CASE("Iterator returned by find and related functions can not be advanced", "[limitation]")
{
    // iterators returned by find, insert, emplace, ...  are just a holder of the found value
    // exception from this are iterators return by begin() methods, they can be used to iterate over
    // the whole table

    sqlitemap sm;

    for (int i = 0; i < 10; i++)
        sm["k" + std::to_string(i)] = "v" + std::to_string(i);
    sm.commit();

    auto it = sm.begin() + 2;
    REQUIRE(it->first == "k2");
    REQUIRE(it->second == "v2");

    auto it_next = ++it;
    REQUIRE(it_next->first == "k3");
    REQUIRE(it_next->second == "v3");

    auto it_found = sm.find("k2");
    REQUIRE(it_found != sm.end());
    REQUIRE(it_found->first == "k2");
    REQUIRE(it_found->second == "v2");

    // would be better when it could be used to advance
    REQUIRE(++it_found == sm.end());

    const auto& csm = sm;
    auto it_found_another = csm.find("k3");
    REQUIRE(it_found_another != csm.end());
    REQUIRE(it_found_another->first == "k3");
    REQUIRE(it_found_another->second == "v3");

    REQUIRE(sm.find("missing-key") == sm.end());
    REQUIRE(csm.find("missing-key") == csm.end());
}

TEST_CASE("Iterator std::next supported")
{
    sqlitemap sm;
    sm["k1"] = "v1";
    sm["k2"] = "v2";
    sm["k3"] = "v3";
    sm["k4"] = "v4";
    sm["k5"] = "v5";

    REQUIRE(*std::next(sm.begin()) == std::pair<std::string, std::string>{"k2", "v2"});

    auto next2 = std::next(sm.begin(), 2);
    REQUIRE(next2 != sm.end());
    REQUIRE(next2->first == "k3");

    auto next1 = std::next(next2);
    REQUIRE(next1 != sm.end());
    REQUIRE(next1->first == "k4");

    REQUIRE_THROWS_AS(std::next(next2, 10), std::out_of_range);
    REQUIRE_THROWS_AS(std::next(sm.end()), std::out_of_range);
}

TEST_CASE("Iterator std::prev not supported")
{
    sqlitemap sm;
    sm["k1"] = "v1";
    sm["k2"] = "v2";
    sm["k3"] = "v3";
    sm["k4"] = "v4";

    auto n1 = std::next(sm.begin());
    REQUIRE(n1->first == "k2");

#ifdef _WIN32
    {
        WARN("MSVC doesn't support calling std::prev for non bidirectional iterators");
    }
#else
    {
        REQUIRE_THROWS_AS(std::prev(n1, 1), std::out_of_range);
    }
#endif
}

TEST_CASE("Reverse iteration")
{
    sqlitemap sm;
    sm["k1"] = "v1";
    sm["k2"] = "v2";
    sm["k3"] = "v3";
    sm["k4"] = "v4";

    std::vector<std::string> keys;
    for (auto it = sm.rbegin(); it != sm.rend(); ++it)
    {
        keys.push_back(it->first);
    }

    REQUIRE(keys == std::vector<std::string>{"k4", "k3", "k2", "k1"});

    const auto& csm = sm;
    std::vector<std::string> ckeys;

    for (auto it = std::next(csm.rbegin(), 2); it != csm.rend(); ++it)
    {
        ckeys.push_back(it->first);
    }
    REQUIRE(ckeys == std::vector<std::string>{"k2", "k1"});
}
TEST_CASE("Const forward iteration")
{
    sqlitemap sm;
    sm["k1"] = "v1";
    sm["k2"] = "v2";
    sm["k3"] = "v3";
    sm["k4"] = "v4";

    using vec = std::vector<decltype(sm)::value_type>;
    vec entries(sm.cbegin(), sm.cend());
    vec expected{{"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}, {"k4", "v4"}};
    REQUIRE(entries == expected);
}

TEST_CASE("Const reverse iteration")
{
    sqlitemap sm;
    sm["k1"] = "v1";
    sm["k2"] = "v2";
    sm["k3"] = "v3";
    sm["k4"] = "v4";

    using vec = std::vector<decltype(sm)::value_type>;
    vec entries(sm.crbegin(), sm.crend());
    vec expected{{"k4", "v4"}, {"k3", "v3"}, {"k2", "v2"}, {"k1", "v1"}};
    REQUIRE(entries == expected);
}

TEST_CASE("Keys forward iteration")
{
    sqlitemap sm(config(key_codec<int>()));

    sm[1] = "v1";
    sm[2] = "v2";
    sm[3] = "v3";
    sm[4] = "v4";

    using vec = std::vector<int>;
    vec entries(sm.keys_begin(), sm.keys_end());
    vec expected{1, 2, 3, 4};
    REQUIRE(entries == expected);
}

TEST_CASE("Keys reverse iteration")
{
    sqlitemap sm(config(key_codec<int>()));

    sm[1] = "v1";
    sm[2] = "v2";
    sm[3] = "v3";
    sm[4] = "v4";

    using vec = std::vector<int>;
    vec entries(sm.keys_rbegin(), sm.keys_rend());
    vec expected{4, 3, 2, 1};
    REQUIRE(entries == expected);
}

TEST_CASE("Const keys forward iteration")
{
    sqlitemap sm(config(key_codec<int>()));

    sm[1] = "v1";
    sm[2] = "v2";
    sm[3] = "v3";
    sm[4] = "v4";

    using vec = std::vector<int>;
    vec entries(sm.keys_cbegin(), sm.keys_cend());
    vec expected{1, 2, 3, 4};
    REQUIRE(entries == expected);
}

TEST_CASE("Const keys reverse iteration")
{
    sqlitemap sm(config(key_codec<int>()));

    sm[1] = "v1";
    sm[2] = "v2";
    sm[3] = "v3";
    sm[4] = "v4";

    using vec = std::vector<int>;
    vec entries(sm.keys_crbegin(), sm.keys_crend());
    vec expected{4, 3, 2, 1};
    REQUIRE(entries == expected);
}

TEST_CASE("values forward iteration")
{
    sqlitemap sm(config(value_codec<int>()));

    sm["v1"] = 1;
    sm["v2"] = 2;
    sm["v3"] = 3;
    sm["v4"] = 4;

    using vec = std::vector<int>;
    vec entries(sm.values_begin(), sm.values_end());
    vec expected{1, 2, 3, 4};
    REQUIRE(entries == expected);
}

TEST_CASE("Values reverse iteration")
{
    sqlitemap sm(config(value_codec<int>()));

    sm["v1"] = 1;
    sm["v2"] = 2;
    sm["v3"] = 3;
    sm["v4"] = 4;

    using vec = std::vector<int>;
    vec entries(sm.values_rbegin(), sm.values_rend());
    vec expected{4, 3, 2, 1};
    REQUIRE(entries == expected);
}

TEST_CASE("Const values forward iteration")
{
    sqlitemap sm(config(value_codec<int>()));

    sm["v1"] = 1;
    sm["v2"] = 2;
    sm["v3"] = 3;
    sm["v4"] = 4;

    using vec = std::vector<int>;
    vec entries(sm.values_cbegin(), sm.values_cend());
    vec expected{1, 2, 3, 4};
    REQUIRE(entries == expected);
}

TEST_CASE("Const values reverse iteration")
{
    sqlitemap sm(config(value_codec<int>()));

    sm["v1"] = 1;
    sm["v2"] = 2;
    sm["v3"] = 3;
    sm["v4"] = 4;

    using vec = std::vector<int>;
    vec entries(sm.values_crbegin(), sm.values_crend());
    vec expected{4, 3, 2, 1};
    REQUIRE(entries == expected);
}

TEST_CASE("Auto commit option")
{
    TempDir temp_dir(Config().enable_logging());
    auto file = (temp_dir.path() / "db.sqlite").string();

    sqlitemap sm_ac(config().filename(file).auto_commit(true));
    sqlitemap client(config().filename(file));

    sm_ac.set("k1", "v1");
    REQUIRE(sm_ac.try_get("k1").value_or("") == "v1");
    REQUIRE(client.try_get("k1").value_or("") == "v1");

    sqlitemap sm_no_ac(config().filename(file).auto_commit(false));
    sm_no_ac.set("k2", "v2");
    REQUIRE(sm_no_ac.try_get("k2").value_or("") == "v2"); // same connections knows changes already
    REQUIRE_FALSE(client.try_get("k2"));

    sm_no_ac.commit(); // after commit changes become visible / synced with filesystem
    REQUIRE(sm_no_ac.try_get("k2").value_or("") == "v2");
    REQUIRE(client.try_get("k2").value_or("") == "v2");
}

TEST_CASE("Rollback when auto_commit is disabled")
{
    TempDir temp_dir(Config().enable_logging());
    auto file = (temp_dir.path() / "db.sqlite").string();

    sqlitemap sm(config().filename(file).auto_commit(false));

    REQUIRE(sm.empty());

    sm.set("k1", "v1");
    sm.commit();

    sm.set("k2", "v2");
    sm.set("k3", "v3");
    sm.rollback(); // rollback all uncommitted changes

    REQUIRE(sm.size() == 1);
    REQUIRE(sm.get("k1") == "v1");

    sm.begin_transaction(); // begin transaction explicitly
    sm.set("k4", "v4");
    sm.set("K5", "v5");
    sm.rollback();

    REQUIRE(sm.size() == 1);
    REQUIRE(sm.get("k1") == "v1");
}

TEST_CASE("Rollback when auto_commit is enabled")
{
    TempDir temp_dir(Config().enable_logging());
    auto file = (temp_dir.path() / "db.sqlite").string();

    sqlitemap sm(config().filename(file).auto_commit(true));

    REQUIRE(sm.empty());

    sm.set("k1", "v1");

    sm.begin_transaction(); // begin transaction explicitly
    sm.set("k2", "v2");
    sm.set("k3", "v3");
    sm.rollback();

    REQUIRE(sm.size() == 1);
    REQUIRE(sm.get("k1") == "v1");

    sm.set("k4", "v4");
    sm.set("k5", "v5");
    sm.rollback(); // has no effect as auto_commit saves all changes immedeatly

    REQUIRE(sm.size() == 3);
    REQUIRE(sm.get("k1") == "v1");
    REQUIRE(sm.get("k4") == "v4");
    REQUIRE(sm.get("k5") == "v5");
}

TEST_CASE("Changes will be committed on close")
{
    TempDir temp_dir(Config().enable_logging());
    auto file = (temp_dir.path() / "db.sqlite").string();

    sqlitemap sm(config().filename(file).auto_commit(false));
    sqlitemap client(config().filename(file).auto_commit(false));

    REQUIRE(sm.empty());
    REQUIRE(client.empty());
    sm.set("k1", "v1");
    sm.set("k2", "v2");
    sm.set("k3", "v3");

    REQUIRE(client.empty());
    sm.close();

    REQUIRE(client.size() == 3);
    REQUIRE(client.get("k1") == "v1");
    REQUIRE(client.get("k2") == "v2");
    REQUIRE(client.get("k3") == "v3");
}

TEST_CASE("sqlitemap can be used to cache big chunks of data")
{
    TempDir td(Config().enable_logging());
    auto file = td.path() / "db.sqlite";

    const size_t num_chars = 1000000; // 1 million characters
    std::string random_string;
    random_string.reserve(num_chars); // Reserve space for efficiency

    std::random_device rd;  // Random device to seed the generator
    std::mt19937 gen(rd()); // Mersenne Twister engine
    std::uniform_int_distribution<> dis('a', 'z');

    // Generate random characters and append them to the string
    for (size_t i = 0; i < num_chars; ++i)
    {
        random_string.push_back(dis(gen));
    }

    { // write
        sqlitemap sm(config().filename(file.string()));
        sm["img1"] = random_string;
        sm["img2"] = random_string;
        sm["img3"] = random_string;
    }
    { // read
        sqlitemap sm(config().filename(file.string()));
        for (auto& [k, v] : sm)
        {
            std::cout << k << " size:" << v.size() << std::endl;
        }

        auto b = sm.begin();
        auto b2 = std::next(b);

        REQUIRE(b->first == "img1");
        REQUIRE(b2->first == "img2");
        REQUIRE(std::next(b2)->first == "img3");
        REQUIRE(std::next(b)->first == "img2");
    }
    { // read keys only
        sqlitemap sm(file.string());
        for (auto kit = sm.keys_begin(); kit != sm.keys_end(); ++kit)
        {
            std::cout << "key: " << *kit << std::endl;
        }
    }
    { // read values only
        sqlitemap sm(file.string());
        for (auto vit = sm.values_begin(); vit != sm.values_end(); ++vit)
        {
            std::cout << "value size: " << vit->size() << std::endl;
        }
    }
}

TEST_CASE("Insert entries from multiple threads")
{
    TempDir temp_dir;

    int num_threads = GENERATE(1, 2, 4);
    int num_entries = 1000;
    int num_entries_per_thread = num_entries / num_threads;

    BENCHMARK("threads: " + std::to_string(num_threads) + " entries:" + std::to_string(num_entries))
    {
        sqlitemap<> sm(config().auto_commit(false));

        std::vector<std::thread> threads;
        for (int t = 1; t <= num_threads; t++)
        {
            threads.push_back(std::thread(
                [&sm, t, num_entries_per_thread]
                {
                    for (int i = 0; i < num_entries_per_thread; i++)
                    {
                        std::string key = std::to_string(t) + "_" + std::to_string(i);
                        std::string value = std::string(i + 1, 'x');
                        sm.set(key, value);
                    }
                }));
        }

        for (auto& t : threads)
            t.join();

        REQUIRE(sm.size() == num_entries);
        return sm;
    };
}

TEST_CASE("Check auto_commit options")
{
    TempDir temp_dir;

    bool auto_commit = GENERATE(false, true);
    int num_entries = 100;

    BENCHMARK("auto_commit:" + std::string(auto_commit ? "true" : "false"))
    {
        sqlitemap sm(config().auto_commit(auto_commit));

        for (int i = 0; i < num_entries; i++)
        {
            std::string key = "k_" + std::to_string(i);
            std::string value = std::string(i + 1, 'x');
            sm.set(key, value);
        }

        REQUIRE(sm.size() == num_entries);
        return sm;
    };
}
