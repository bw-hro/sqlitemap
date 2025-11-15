// sqlitemap
// SPDX-FileCopyrightText: 2024-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <bw/sqlitemap/sqlitemap.hpp>
#include <catch2/catch_all.hpp>
#include <thread>

#include <bw/tempdir/tempdir.hpp>

#include "conversion_functor.hpp"

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
    auto smc_ptr = std::make_unique<sqlitemap_t<int, double>>(config<int, double>());

    // as member of a class
    class App
    {
        using DB = sqlitemap<>;

      public:
        App(const std::string& file)
            : db(file, "cache")
        {
            db.set("app-1", "123");
            db.commit();
        }

      private:
        DB db;
    };

    TempDir temp_dir(Config().enable_logging());
    auto file = temp_dir.path() / "db.sqlite";

    REQUIRE_NOTHROW(App(file.string()));
    REQUIRE((sqlitemap(file.string(), "cache", operation_mode::r).get("app-1") == "123"));
}

TEST_CASE("sqlitemap move constructor and assignment")
{
    TempDir temp_dir(Config().enable_logging());
    auto file = (temp_dir.path() / "db.sqlite").string();

    sqlitemap sm1(config().filename(file).log_level(log_level::trace));
    sm1.set("k1", "v1");
    sm1.commit();

    {
        // move constructor
        sqlitemap sm2(std::move(sm1));
        REQUIRE(sm2.size() == 1);
        REQUIRE((sm2["k1"] == "v1"));
        sm2.set("k2", "v2");
        sm2.commit();

        // move assignment
        sqlitemap sm3;
        sm3 = std::move(sm2);
        REQUIRE(sm3.size() == 2);
        REQUIRE((sm3["k1"] == "v1"));
        REQUIRE((sm3["k2"] == "v2"));
        sm3.set("k3", "v3");
        sm3.commit();
    }

    sqlitemap sm4(config().filename(file).log_level(log_level::trace));
    REQUIRE(sm4.size() == 3);
    REQUIRE((sm4["k1"] == "v1"));
    REQUIRE((sm4["k2"] == "v2"));
    REQUIRE((sm4["k3"] == "v3"));
    sm4.set("k4", "v4");
    sm4.commit();

    // move assignment, reuse sm1, which was moved from before
    sm1 = std::move(sm4);
    REQUIRE(sm1.size() == 4);
    REQUIRE((sm1["k1"] == "v1"));
    REQUIRE((sm1["k2"] == "v2"));
    REQUIRE((sm1["k3"] == "v3"));
    REQUIRE((sm1["k4"] == "v4"));
}

TEST_CASE("sqlitemap can be stored in a std::vector")
{
    TempDir temp_dir(Config().enable_logging());
    auto file1 = (temp_dir.path() / "db1.sqlite").string();
    auto file2 = (temp_dir.path() / "db2.sqlite").string();

    std::vector<sqlitemap<>> db_vector;
    db_vector.emplace_back(config().filename(file1).log_level(log_level::debug));
    db_vector.push_back(sqlitemap(config().filename(file2).log_level(log_level::debug)));

    db_vector[0].set("k1", "v1");
    db_vector[0].commit();

    auto& db2 = db_vector[1];
    db2.set("k2", "v2");
    db2.commit();

    for (auto& db : db_vector)
    {
        db.set("loop_key", "loop_value");
        db.commit();
    }

    REQUIRE(db_vector[0].get("k1") == "v1");
    REQUIRE(db_vector[0].get("loop_key") == "loop_value");

    REQUIRE((db2["k2"] == "v2"));
    REQUIRE((db2["loop_key"] == "loop_value"));
}

TEST_CASE("sqlitemap can be stored in a std::map")
{
    TempDir temp_dir(Config().enable_logging());
    auto file1 = (temp_dir.path() / "db1.sqlite").string();
    auto file2 = (temp_dir.path() / "db2.sqlite").string();

    std::map<std::string, sqlitemap<>> db_map;

    db_map["first"] = sqlitemap<>(config().filename(file1).log_level(log_level::debug));
    db_map["second"] = sqlitemap<>(config().filename(file2).log_level(log_level::debug));

    db_map["first"].set("k1", "v1");
    db_map["first"].commit();

    auto& db2 = db_map["second"];
    db2.set("k2", "v2");
    db2.commit();

    REQUIRE(db_map["first"].get("k1") == "v1");
    REQUIRE((db2["k2"] == "v2"));
}

TEST_CASE("sqlitemap tries to avoid copies of configuration and codecs")
{
    using namespace bw::testhelper;

    // clang-format off
    struct key_codec_encode_functor : public conversion_functor<int, std::string>
    {
        key_codec_encode_functor(counts* counts_ptr = nullptr): conversion_functor<int, std::string>(
            "KEY_ENCODE_FUNCTOR", [](int key){ return "key-" + std::to_string(key); }, counts_ptr)
        {}
    };
    
    struct key_codec_decode_functor : public conversion_functor<std::string, int>
    {
        key_codec_decode_functor(counts* counts_ptr = nullptr): conversion_functor<std::string, int>(
            "KEY_DECODE_FUNCTOR", [](std::string key_str){ return std::atoi(key_str.substr(4).c_str()); }, counts_ptr)
        {}
    };

    struct value_codec_encode_functor : public conversion_functor<double, std::string>
    {
        value_codec_encode_functor(counts* counts_ptr = nullptr): conversion_functor<double, std::string>(
            "VALUE_ENCODE_FUNCTOR", [](double value){ return "value-" +  std::to_string(value); }, counts_ptr)
        {}
    };

    struct value_codec_decode_functor : public conversion_functor<std::string, double>
    {
        value_codec_decode_functor(counts* counts_ptr = nullptr): conversion_functor<std::string, double>(
            "VALUE_DECODE_FUNCTOR", [](std::string value_str){ return std::atof(value_str.substr(6).c_str()); }, counts_ptr)
        {}
    };
    
    struct test_config: public instance_counter<test_config>,
        public configuration<codecs::codec_pair<
        key_codec_t<int, std::string>, value_codec_t<double, std::string>>>
    {
        using counter = instance_counter<test_config>;

        test_config(counts* counts_ptr = nullptr)
            : kc_encode_counts()
            , kc_decode_counts()
            , vc_encode_counts()
            , vc_decode_counts()
            , counter("######### TEST_CONFIG", counts_ptr)
            , configuration(config(
                key_codec(
                    key_codec_encode_functor{&kc_encode_counts},
                    key_codec_decode_functor{&kc_decode_counts}),
                value_codec(
                    value_codec_encode_functor{&vc_encode_counts},
                    value_codec_decode_functor{&vc_decode_counts})))
        {
        }

        counts kc_encode_counts;
        counts kc_decode_counts;
        counts vc_encode_counts;
        counts vc_decode_counts;
    };

    // clang-format on

    { // check that copies are avoided when passing configuration as rvalue reference
        counts test_config_counts;
        {
            sqlitemap sm(test_config{&test_config_counts});
            sm.set(42, 3.1415);
            REQUIRE(sm.get(42) == Catch::Approx(3.1415));
        }

        REQUIRE(test_config_counts.default_ctor_count == 1);
        REQUIRE(test_config_counts.copy_ctor_count == 0);
        REQUIRE(test_config_counts.copy_assign_count == 0);
        REQUIRE(test_config_counts.dtor_count == 1);
    }

    { // check that copies are avoided when passing configuration and codecs as rvalue references
        counts kc_encode_counts;
        counts kc_decode_counts;
        counts vc_encode_counts;
        counts vc_decode_counts;

        {
            sqlitemap sm(config(key_codec(key_codec_encode_functor{&kc_encode_counts},
                                          key_codec_decode_functor{&kc_decode_counts}),
                                value_codec(value_codec_encode_functor{&vc_encode_counts},
                                            value_codec_decode_functor{&vc_decode_counts})));
            sm.set(42, 3.1415);
            sm.set(24, 2.4);
            sm.set(1, 42);
            sm.commit();

            REQUIRE(sm.get(42) == Catch::Approx(3.1415));
            REQUIRE(sm.get(24) == Catch::Approx(2.4));
            REQUIRE(sm.get(1) == Catch::Approx(42));

            for (const auto& [key, value] : sm)
            {
                std::cout << "key:" << key << " value:" << value << "\n";
            }

            auto res = std::find_if(sm.begin(), sm.end(), [](auto kv) { return kv.second > 10; });
            REQUIRE(res != sm.end());
            REQUIRE(res->first == 1);
        }

        REQUIRE(kc_encode_counts.default_ctor_count == 1);
        REQUIRE(kc_decode_counts.default_ctor_count == 1);
        REQUIRE(vc_encode_counts.default_ctor_count == 1);
        REQUIRE(vc_decode_counts.default_ctor_count == 1);

        REQUIRE(kc_encode_counts.copy_ctor_count == 0);
        REQUIRE(kc_decode_counts.copy_ctor_count == 0);
        REQUIRE(vc_encode_counts.copy_ctor_count == 0);
        REQUIRE(vc_decode_counts.copy_ctor_count == 0);

        REQUIRE(kc_encode_counts.copy_assign_count == 0);
        REQUIRE(kc_decode_counts.copy_assign_count == 0);
        REQUIRE(vc_encode_counts.copy_assign_count == 0);
        REQUIRE(vc_decode_counts.copy_assign_count == 0);

        REQUIRE(kc_encode_counts.dtor_count == 1);
        REQUIRE(kc_decode_counts.dtor_count == 1);
        REQUIRE(vc_encode_counts.dtor_count == 1);
        REQUIRE(vc_decode_counts.dtor_count == 1);
    }

    { // check that copies are avoided when passing configuration
      // as rvalue and codecs as lvalue references
        counts kc_encode_counts;
        counts kc_decode_counts;
        counts vc_encode_counts;
        counts vc_decode_counts;

        {
            auto kc = key_codec(key_codec_encode_functor{&kc_encode_counts},
                                key_codec_decode_functor{&kc_decode_counts});

            auto vc = value_codec(value_codec_encode_functor{&vc_encode_counts},
                                  value_codec_decode_functor{&vc_decode_counts});

            sqlitemap sm(config(kc, vc)); // only copy of codecs, configuration is passed as rvalue
            sm.set(42, 3.1415);
            sm.set(24, 2.4);
            sm.set(1, 42);
            sm.commit();

            REQUIRE(sm.get(42) == Catch::Approx(3.1415));
            REQUIRE(sm.get(24) == Catch::Approx(2.4));
            REQUIRE(sm.get(1) == Catch::Approx(42));

            for (const auto& [key, value] : sm)
            {
                std::cout << "key:" << key << " value:" << value << "\n";
            }

            auto res = std::find_if(sm.begin(), sm.end(), [](auto kv) { return kv.second > 10; });
            REQUIRE(res != sm.end());
            REQUIRE(res->first == 1);
        }

        REQUIRE(kc_encode_counts.default_ctor_count == 1);
        REQUIRE(kc_decode_counts.default_ctor_count == 1);
        REQUIRE(vc_encode_counts.default_ctor_count == 1);
        REQUIRE(vc_decode_counts.default_ctor_count == 1);

        REQUIRE(kc_encode_counts.copy_ctor_count == 1);
        REQUIRE(kc_decode_counts.copy_ctor_count == 1);
        REQUIRE(vc_encode_counts.copy_ctor_count == 1);
        REQUIRE(vc_decode_counts.copy_ctor_count == 1);

        REQUIRE(kc_encode_counts.copy_assign_count == 0);
        REQUIRE(kc_decode_counts.copy_assign_count == 0);
        REQUIRE(vc_encode_counts.copy_assign_count == 0);
        REQUIRE(vc_decode_counts.copy_assign_count == 0);

        // 2 d'tor calls: 1 for original, 1 copy
        REQUIRE(kc_encode_counts.dtor_count == 2);
        REQUIRE(kc_decode_counts.dtor_count == 2);
        REQUIRE(vc_encode_counts.dtor_count == 2);
        REQUIRE(vc_decode_counts.dtor_count == 2);
    }

    { // check that copies are avoided when passing configuration and codecs as lvalue references
        counts kc_encode_counts;
        counts kc_decode_counts;
        counts vc_encode_counts;
        counts vc_decode_counts;

        {
            auto kc = key_codec(key_codec_encode_functor{&kc_encode_counts},
                                key_codec_decode_functor{&kc_decode_counts});

            auto vc = value_codec(value_codec_encode_functor{&vc_encode_counts},
                                  value_codec_decode_functor{&vc_decode_counts});

            auto cfg = config(kc, vc); // first copy of codecs

            sqlitemap sm(cfg); // second copy of codecs, as the whole configuration is copied
            sm.set(42, 3.1415);
            sm.set(24, 2.4);
            sm.set(1, 42);
            sm.commit();

            REQUIRE(sm.get(42) == Catch::Approx(3.1415));
            REQUIRE(sm.get(24) == Catch::Approx(2.4));
            REQUIRE(sm.get(1) == Catch::Approx(42));

            for (const auto& [key, value] : sm)
            {
                std::cout << "key:" << key << " value:" << value << "\n";
            }

            auto res = std::find_if(sm.begin(), sm.end(), [](auto kv) { return kv.second > 10; });
            REQUIRE(res != sm.end());
            REQUIRE(res->first == 1);
        }

        REQUIRE(kc_encode_counts.default_ctor_count == 1);
        REQUIRE(kc_decode_counts.default_ctor_count == 1);
        REQUIRE(vc_encode_counts.default_ctor_count == 1);
        REQUIRE(vc_decode_counts.default_ctor_count == 1);

        REQUIRE(kc_encode_counts.copy_ctor_count == 2);
        REQUIRE(kc_decode_counts.copy_ctor_count == 2);
        REQUIRE(vc_encode_counts.copy_ctor_count == 2);
        REQUIRE(vc_decode_counts.copy_ctor_count == 2);

        REQUIRE(kc_encode_counts.copy_assign_count == 0);
        REQUIRE(kc_decode_counts.copy_assign_count == 0);
        REQUIRE(vc_encode_counts.copy_assign_count == 0);
        REQUIRE(vc_decode_counts.copy_assign_count == 0);

        // 3 d'tor calls: 1 for original, 2 copies
        REQUIRE(kc_encode_counts.dtor_count == 3);
        REQUIRE(kc_decode_counts.dtor_count == 3);
        REQUIRE(vc_encode_counts.dtor_count == 3);
        REQUIRE(vc_decode_counts.dtor_count == 3);
    }

    {
        counts kc_encode_counts;
        counts kc_decode_counts;
        counts vc_encode_counts;
        counts vc_decode_counts;

        struct db_wrapper
        {
            using kc_t = key_codec_t<int, std::string>;
            using vc_t = value_codec_t<double, std::string>;
            using DB = sqlitemap_t<kc_t, vc_t>;

            db_wrapper(std::string table_name,                             //
                       counts* kc_encode_counts, counts* kc_decode_counts, //
                       counts* vc_encode_counts, counts* vc_decode_counts)
                : _db(config(key_codec(key_codec_encode_functor{kc_encode_counts},
                                       key_codec_decode_functor{kc_decode_counts}),
                             value_codec(value_codec_encode_functor{vc_encode_counts},
                                         value_codec_decode_functor{vc_decode_counts}))
                          .table(table_name))
            {
                _db.set(42, 3.1415);
                _db.set(24, 2.4);
                _db.set(1, 42);
                _db.commit();
            }

            int stored_records() const
            {
                return static_cast<int>(_db.size());
            }

          private:
            DB _db;
        };

        {
            db_wrapper db("test_table",                         //
                          &kc_encode_counts, &kc_decode_counts, //
                          &vc_encode_counts, &vc_decode_counts);
            REQUIRE(db.stored_records() == 3);
        }

        REQUIRE(kc_encode_counts.default_ctor_count == 1);
        REQUIRE(kc_decode_counts.default_ctor_count == 1);
        REQUIRE(vc_encode_counts.default_ctor_count == 1);
        REQUIRE(vc_decode_counts.default_ctor_count == 1);

        REQUIRE(kc_encode_counts.copy_ctor_count == 0);
        REQUIRE(kc_decode_counts.copy_ctor_count == 0);
        REQUIRE(vc_encode_counts.copy_ctor_count == 0);
        REQUIRE(vc_decode_counts.copy_ctor_count == 0);

        REQUIRE(kc_encode_counts.copy_assign_count == 0);
        REQUIRE(kc_decode_counts.copy_assign_count == 0);
        REQUIRE(vc_encode_counts.copy_assign_count == 0);
        REQUIRE(vc_decode_counts.copy_assign_count == 0);

        REQUIRE(kc_encode_counts.dtor_count == 1);
        REQUIRE(kc_decode_counts.dtor_count == 1);
        REQUIRE(vc_encode_counts.dtor_count == 1);
        REQUIRE(vc_decode_counts.dtor_count == 1);
    }
}

TEST_CASE("sqlitemap configuration offers a fluent interface")
{
    // configure all options using rvalue method calls
    auto cfg = config()
                   .filename("test_db.sqlite")
                   .table("test_table")
                   .mode(operation_mode::w)
                   .auto_commit(true)
                   .log_level(log_level::debug)
                   .log_impl([](auto level, auto msg) {})
                   .pragma("journal_mode", "WAL")
                   .pragma("cache_size", -64000)
                   .pragma("temp_store = 2");

    REQUIRE(cfg.filename() == "test_db.sqlite");
    REQUIRE(cfg.table() == "test_table");
    REQUIRE(cfg.mode() == operation_mode::w);
    REQUIRE(cfg.auto_commit());
    REQUIRE(cfg.log_level() == log_level::debug);
    REQUIRE(cfg.pragmas() == std::vector<std::string>{"PRAGMA journal_mode = WAL",
                                                      "PRAGMA cache_size = -64000",
                                                      "PRAGMA temp_store = 2"});

    // configure all options using lvalue method calls
    auto cfg2 = config();
    cfg2.filename("test_db2.sqlite");
    cfg2.table("test_table2");
    cfg2.mode(operation_mode::r);
    cfg2.auto_commit(false);
    cfg2.log_level(log_level::error);
    cfg2.log_impl([](auto level, auto msg) {});
    cfg2.pragma("cache_size", 2000);
    cfg2.pragma("synchronous", "OFF");

    REQUIRE(cfg2.filename() == "test_db2.sqlite");
    REQUIRE(cfg2.table() == "test_table2");
    REQUIRE(cfg2.mode() == operation_mode::r);
    REQUIRE_FALSE(cfg2.auto_commit());
    REQUIRE(cfg2.log_level() == log_level::error);
    REQUIRE(cfg2.pragmas() ==
            std::vector<std::string>{"PRAGMA cache_size = 2000", "PRAGMA synchronous = OFF"});
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
        sm.commit();
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
        sm.commit();
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
        sm.commit();
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

    // operator[] can be used as input for operator<<
    sm["k1"] = "v1";
    std::ostringstream oss;
    oss << "k1=" << sm["k1"];
    REQUIRE(oss.str() == "k1=v1");
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
    WARN("MSVC doesn't support calling std::prev for non bidirectional iterators");
#elif defined(__APPLE__) && defined(TARGET_OS_MAC)
    WARN("Clang on macOS doesn't support calling std::prev for non bidirectional iterators");
#else
    REQUIRE_THROWS_AS(std::prev(n1, 1), std::out_of_range);
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

TEST_CASE("Changes will be committed on close when auto commit mode is active")
{
    auto kc = key_codec<std::string>();
    auto vc = value_codec<std::string>();
    using codecs_pair_t = codecs::codec_pair<decltype(kc), decltype(vc)>;

    TempDir temp_dir(Config().enable_logging());
    auto file = (temp_dir.path() / "db.sqlite").string();

    // auto commit is disabled
    sqlitemap<codecs_pair_t> sm(config(kc, vc).filename(file).auto_commit(false));
    sqlitemap<codecs_pair_t> client(config(kc, vc).filename(file).auto_commit(false));

    REQUIRE(sm.empty());
    REQUIRE(client.empty());
    sm.set("k1", "v1");
    sm.set("k2", "v2");
    sm.set("k3", "v3");

    REQUIRE(client.empty());

    auto& mutable_config = const_cast<configuration<codecs_pair_t>&>(sm.config());
    mutable_config.auto_commit(true); // enable auto commit
    sm.close();

    REQUIRE(client.size() == 3);
    REQUIRE(client.get("k1") == "v1");
    REQUIRE(client.get("k2") == "v2");
    REQUIRE(client.get("k3") == "v3");
}

TEST_CASE("Changes will be discarded on close when not committed")
{
    TempDir temp_dir(Config().enable_logging());
    auto file = (temp_dir.path() / "db.sqlite").string();

    // auto commit is disabled
    sqlitemap sm(config().filename(file).auto_commit(false));
    sqlitemap client(config().filename(file).auto_commit(false));

    REQUIRE(sm.empty());
    REQUIRE(client.empty());
    sm.set("k1", "v1");
    sm.commit();
    sm.set("k2", "v2");
    sm.set("k3", "v3");

    REQUIRE(client.size() == 1);
    REQUIRE(client.get("k1") == "v1");
    sm.close();

    REQUIRE(client.size() == 1);
    REQUIRE(client.get("k1") == "v1");

    sqlitemap another_client(config().filename(file).auto_commit(false));
    REQUIRE(another_client.size() == 1);
    REQUIRE(another_client.get("k1") == "v1");
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
        sqlitemap sm(config().filename(file.string()).auto_commit(true));
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

TEST_CASE("PRAGMA statements can be used to set SQLite options")
{
    auto get_db_option = [](sqlite3* db, const std::string& pragma_statement) -> std::string
    {
        auto pragma_callback = [](void* result, int argc, char** argv, char** col_name)
        {
            std::string* out = static_cast<std::string*>(result);
            *out = argv[0];
            return 0;
        };
        std::string result;
        details::exec_checked(db, pragma_statement, pragma_callback, &result);
        return result;
    };

    TempDir temp_dir(Config().enable_logging());

    // check default options
    auto file_default = (temp_dir.path() / "db_default.sqlite").string();
    sqlitemap sm_default(config().filename(file_default));

    REQUIRE(get_db_option(sm_default.get_connection(), "PRAGMA journal_mode") == "delete");
    REQUIRE(get_db_option(sm_default.get_connection(), "PRAGMA cache_size") == "-2000");
    REQUIRE(get_db_option(sm_default.get_connection(), "PRAGMA temp_store") == "0");
    REQUIRE(get_db_option(sm_default.get_connection(), "PRAGMA synchronous") == "2");

    sm_default.set("k1", "v1");
    sm_default.commit();
    REQUIRE(sm_default.size() == 1);
    REQUIRE(sm_default.get("k1") == "v1");

    // check custom options
    auto file_custom = (temp_dir.path() / "db_custom.sqlite").string();
    sqlitemap sm_custom(
        config()
            .filename(file_custom)
            .pragma("journal_mode", "WAL") // DELETE | TRUNCATE | PERSIST | MEMORY | WAL | OFF
            .pragma("cache_size", -4000)   // -4000 = 4000KiB, 4000 = number of pages
            .pragma("temp_store = 2")      // 0 = DEFAULT, 1 = FILE, 2 = MEMORY
            .pragma("PRAGMA synchronous = NORMAL")); // 0 = OFF, 1 = NORMAL, 2 = FULL

    REQUIRE(get_db_option(sm_custom.get_connection(), "PRAGMA journal_mode") == "wal");
    REQUIRE(get_db_option(sm_custom.get_connection(), "PRAGMA cache_size") == "-4000");
    REQUIRE(get_db_option(sm_custom.get_connection(), "PRAGMA temp_store") == "2");
    REQUIRE(get_db_option(sm_custom.get_connection(), "PRAGMA synchronous") == "1");

    sm_custom.set("k1", "v1");
    sm_custom.commit();
    REQUIRE(sm_custom.size() == 1);
    REQUIRE(sm_custom.get("k1") == "v1");
}
