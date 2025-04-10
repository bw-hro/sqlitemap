// sqlitemap
// SPDX-FileCopyrightText: 2024-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <catch2/catch_all.hpp>

#include <bw/tempdir/tempdir.hpp>

#include "custom.hpp"
#include <bw/sqlitemap/sqlitemap.hpp>

using namespace bw::sqlitemap;

using namespace bw::tempdir;
namespace fs = std::filesystem;

TEST_CASE("some types can be converted to strings, some not", "[codecs]")
{
    int i = 42;
    double d = 1.23;
    std::string s = "string-value";
    const char* c = "char*-value";
    bw::testhelper::custom custom_object{42};
    struct another_custom
    {
    } another_custom;

    REQUIRE(details::as_string_or(i) == "42");
    REQUIRE(details::as_string_or(d).substr(0, 4) == "1.23");
    REQUIRE(details::as_string_or(s) == "string-value");
    REQUIRE(details::as_string_or(c) == "char*-value");
    REQUIRE(details::as_string_or(custom_object) == "custom[42]");
    REQUIRE(details::as_string_or(another_custom, "n/a") == "n/a");
    // by default type name will be used, which might be compiler dependend
    REQUIRE(details::as_string_or(another_custom).substr(0, 5) == "type:");
}

TEST_CASE("sqlitemap maps c++ types to sqlite storage classes", "[codecs]")
{
    using sc = codecs::sqlite_storage_class;

    REQUIRE(codecs::to_string(sc::INTEGER) == "INTEGER");
    REQUIRE(codecs::to_string(sc::REAL) == "REAL");
    REQUIRE(codecs::to_string(sc::TEXT) == "TEXT");
    REQUIRE(codecs::to_string(sc::BLOB) == "BLOB");

    REQUIRE(codecs::sqlite_storage_class_from_type<int>() == sc::INTEGER);
    REQUIRE(codecs::sqlite_storage_class_from_type<long>() == sc::INTEGER);
    REQUIRE(codecs::sqlite_storage_class_from_type<float>() == sc::REAL);
    REQUIRE(codecs::sqlite_storage_class_from_type<double>() == sc::REAL);
    REQUIRE(codecs::sqlite_storage_class_from_type<std::string>() == sc::TEXT);
    REQUIRE(codecs::sqlite_storage_class_from_type<const char*>() == sc::TEXT);
    REQUIRE(codecs::sqlite_storage_class_from_type<std::vector<std::byte>>() == sc::BLOB);
}

TEST_CASE("sqlitemap uses string to string conversion as default codecs for key and value",
          "[codecs]")
{
    sqlitemap sm;

    std::string encoded_key = sm.config().codecs().key_codec.encode("key-1");
    std::string decoded_key = sm.config().codecs().key_codec.decode(encoded_key);

    std::string encoded_value = sm.config().codecs().value_codec.encode("value-1");
    std::string decoded_value = sm.config().codecs().value_codec.decode(encoded_value);

    REQUIRE(encoded_key == "key-1");
    REQUIRE(decoded_key == "key-1");
    REQUIRE(encoded_value == "value-1");
    REQUIRE(decoded_value == "value-1");
}

namespace test
{

std::string encode_value(const std::string& value)
{
    return "PREFIX_" + value;
}

std::string decode_value(const std::string& value)
{
    return value.substr(std::string("PREFIX_").length());
}

std::string encode_key(const int& key)
{
    std::ostringstream oss;
    oss << (key + 1000);
    return oss.str();
}

int decode_key(std::string key)
{
    return std::atoi(key.c_str()) - 1000;
}

} // namespace test

TEST_CASE("sqlitemap can be configured to use custom codecs", "[codecs]")
{
    auto kc = key_codec(test::encode_key, test::decode_key);
    auto vc = value_codec(test::encode_value, test::decode_value);

    // custom key and value codecs

    sqlitemap sm_kv(config(kc, vc));

    auto encoded_key_kv = sm_kv.config().codecs().key_codec.encode(42);
    auto decoded_key_kv = sm_kv.config().codecs().key_codec.decode(encoded_key_kv);

    std::string encoded_value_kv = sm_kv.config().codecs().value_codec.encode("value-1");
    std::string decoded_value_kv = sm_kv.config().codecs().value_codec.decode(encoded_value_kv);

    REQUIRE(encoded_key_kv == "1042");
    REQUIRE(decoded_key_kv == 42);
    REQUIRE(encoded_value_kv == "PREFIX_value-1");
    REQUIRE(decoded_value_kv == "value-1");

    sm_kv.set(123, "val-123");
    sm_kv[456] = "val-456";
    REQUIRE(sm_kv.get(123) == "val-123");
    REQUIRE((sm_kv[123] == "val-123"));

    // custom key codec

    sqlitemap sm_k(config(kc));

    auto encoded_key_k = sm_k.config().codecs().key_codec.encode(42);
    auto decoded_key_k = sm_k.config().codecs().key_codec.decode(encoded_key_k);

    std::string encoded_value_k = sm_k.config().codecs().value_codec.encode("value-1");
    std::string decoded_value_k = sm_k.config().codecs().value_codec.decode(encoded_value_k);

    REQUIRE(encoded_key_k == "1042");
    REQUIRE(decoded_key_k == 42);
    REQUIRE(encoded_value_k == "value-1");
    REQUIRE(decoded_value_k == "value-1");

    sm_k.set(3, "val-3");
    sm_k[4] = "val-4";
    REQUIRE(sm_k.get(3) == "val-3");
    REQUIRE((sm_k[4] == "val-4"));

    // custom value codec

    sqlitemap sm_v(config(vc));

    auto encoded_key_v = sm_v.config().codecs().key_codec.encode("4213");
    auto decoded_key_v = sm_v.config().codecs().key_codec.decode(encoded_key_v);

    std::string encoded_value_v = sm_v.config().codecs().value_codec.encode("value-1");
    std::string decoded_value_v = sm_v.config().codecs().value_codec.decode(encoded_value_v);

    REQUIRE(encoded_key_v == "4213");
    REQUIRE(decoded_key_v == "4213");
    REQUIRE(encoded_value_v == "PREFIX_value-1");
    REQUIRE(decoded_value_v == "value-1");

    sm_v.set("key-7", "val-7");
    sm_v["key-8"] = "val-8";
    REQUIRE(sm_v.get("key-7") == "val-7");
    REQUIRE((sm_v["key-8"] == "val-8"));
}

TEST_CASE("custom codecs can be specified as lambdas", "[codecs]")
{
    using namespace bw::testhelper;

    auto kc = key_codec([](point p) { return point::to_string(p); },
                        [](std::string s) { return point::from_string(s); });

    auto vc = value_codec([](feature f) { return feature::to_string(f); },
                          [](std::string s) { return feature::from_string(s); });

    // custom key and value codecs

    sqlitemap sm(config(kc, vc));

    sm.set({0, 0, 0}, {"origin", 5});
    sm[{1, 0, 0}] = {"x-direction", 1};
    REQUIRE(sm.get({1, 0, 0}) == feature{"x-direction", 1});
    REQUIRE((sm[{0, 0, 0}] == feature{"origin", 5}));
}

TEST_CASE("custom codecs used in iterator", "[codecs]")
{
    using namespace bw::testhelper;

    auto kc = key_codec(point::to_string, point::from_string);
    auto vc = value_codec(feature::to_string, feature::from_string);

    // custom key and value codecs

    sqlitemap sm(config(kc, vc));

    REQUIRE_NOTHROW(sm.set({1, 0, 50}, {"feature-1", 1}));
    REQUIRE_NOTHROW(sm.set({2, 0, 40}, {"feature-2", 2}));
    REQUIRE_NOTHROW(sm.set({3, 0, 30}, {"feature-3", 3}));
    REQUIRE_NOTHROW(sm.set({4, 0, 20}, {"feature-4", 4}));
    REQUIRE_NOTHROW(sm.set({5, 0, 10}, {"feature-5", 5}));

    REQUIRE(sm.size() == 5);

    auto pf3 = sm.begin() + 2;
    REQUIRE(pf3->first == point{3, 0, 30});

    int summed_rating = 0;
    for (auto [p, f] : sm)
    {
        std::cout << point::to_string(p) << " : " << feature::to_string(f) << std::endl;
        summed_rating += f.rating;
    }
    REQUIRE(summed_rating == 15);
}

TEST_CASE("codecs can be defined by type identity function", "[codecs]")
{
    auto kc = key_codec<int>();
    auto vc = value_codec<double>();

    auto sm = sqlitemap(config(kc, vc));

    REQUIRE_NOTHROW(sm.set(42, 0.1234));
    REQUIRE(sm.get(42) == Catch::Approx(0.1234));

    REQUIRE_NOTHROW(sm.del(42));
    REQUIRE_THROWS_AS(sm.get(42), sqlitemap_error);

    REQUIRE_NOTHROW(sm.insert({{1, 9.111}, {2, 8.222}, {3, 7.333}}));
    REQUIRE(sm.get(1) == Catch::Approx(9.111));
    REQUIRE(sm.get(2) == Catch::Approx(8.222));
    REQUIRE(sm.get(3) == Catch::Approx(7.333));
}

template <typename T> T create_test_value()
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        return "test-string";
    }
    if constexpr (std::is_same_v<T, bool>)
    {
        return true;
    }
    else if constexpr (std::is_integral_v<T>)
    {
        return 42;
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
        return 42.123;
    }
    else if constexpr (std::is_same_v<T, std::vector<std::byte>>)
    {
        return std::vector<std::byte>({std::byte{'x'}, std::byte{'o'}, std::byte{'x'}});
    };
}

TEMPLATE_TEST_CASE("sqlitemap detects storage type", "[codecs]", //
                   bool, int, long, long long, float, double, std::string, std::vector<std::byte>)
{
    // test each type could be used as key and value

    auto kc = key_codec<TestType>();
    auto vc = value_codec<TestType>();
    sqlitemap sm_kv(config(kc, vc));
    REQUIRE(sm_kv.empty());

    // try to store default value

    sqlitemap sm_v(config(vc));
    auto value = create_test_value<TestType>();

    REQUIRE_NOTHROW(sm_v.set("key", value));
    REQUIRE(sm_v.get("key") == value);
}

TEST_CASE("what happens when sqlitemap is opened with wrong storage type?", "[codecs]")
{
    TempDir temp_dir(Config().enable_logging());
    std::string file = (temp_dir.path() / "db.sqlite").string();

    { // open db int -> double
        sqlitemap sm(config<int, double>().filename(file));
        REQUIRE_NOTHROW(sm.set(1, 4.2));
        REQUIRE(sm.get(1) == Catch::Approx(4.2));
    }

    { // open db int -> std::string
        sqlitemap sm(config<int, std::string>().filename(file));
        REQUIRE_NOTHROW(sm.set(2, "John Doe"));
        REQUIRE((sm.get(2) == "John Doe"));
        REQUIRE((sm[1] == "4.2")); // double will be converted automatically to std::string
    }

    { // open db int -> double again
        sqlitemap sm(config<int, double>().filename(file));
        REQUIRE(sm.get(1) == Catch::Approx(4.2));
        REQUIRE(sm.get(2) == Catch::Approx(0)); // name can not be converted to int, falls back to 0
    }
}

TEST_CASE("use std::variant to store diffent type in one table")
{
    auto encode = [](const std::variant<int, std::string>& value)
    {
        if (std::holds_alternative<int>(value))
            return "int:" + std::to_string(std::get<int>(value));
        else
            return "string:" + std::get<std::string>(value);
    };

    auto decode = [](const std::string& value) -> std::variant<int, std::string>
    {
        if (value[0] == 'i')
            return std::atoi(value.substr(4).c_str()); // remove "int:"
        else
            return value.substr(7); // remove "string:"
    };

    auto vc = value_codec(encode, decode);
    sqlitemap sm(config(vc));

    sm.set("k1", "Hello World!");
    sm.set("k2", 42);

    REQUIRE(std::get<std::string>(sm.get("k1")) == "Hello World!");
    REQUIRE(std::get<int>(sm.get("k2")) == 42);
}
