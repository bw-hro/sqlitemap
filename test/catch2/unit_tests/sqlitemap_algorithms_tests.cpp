// sqlitemap
// SPDX-FileCopyrightText: 2024-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <bw/sqlitemap/sqlitemap.hpp>
#include <catch2/catch_all.hpp>

#include <bw/tempdir/tempdir.hpp>
#include <numeric>

using namespace bw::sqlitemap;

using namespace bw::tempdir;
namespace fs = std::filesystem;

TEST_CASE("std::for_each")
{
    sqlitemap sm;
    sm["k1"] = "1";
    sm["k2"] = "2";
    sm["k3"] = "3";

    std::string concat;
    std::for_each(sm.begin(), sm.end(), [&concat](auto& kv) { concat += kv.second; });
    REQUIRE(concat == "123");

    std::string concat_c;
    std::for_each(sm.cbegin(), sm.cend(), [&concat_c](auto& kv) { concat_c += kv.second; });
    REQUIRE(concat_c == "123");

    std::string concat_r;
    std::for_each(sm.rbegin(), sm.rend(), [&concat_r](auto& kv) { concat_r += kv.second; });
    REQUIRE(concat_r == "321");

    std::string concat_cr;
    std::for_each(sm.crbegin(), sm.crend(), [&concat_cr](auto& kv) { concat_cr += kv.second; });
    REQUIRE(concat_cr == "321");
}

TEST_CASE("std::distance")
{
    sqlitemap sm;
    sm["k1"] = "x";
    sm["k2"] = "xx";
    sm["k3"] = "xxx";
    sm["k4"] = "xxxx";
    sm["k5"] = "xxxxx";

    REQUIRE(std::distance(sm.begin(), sm.end()) == 5);

    auto start = sm.begin() + 1;
    REQUIRE(std::distance(start, start + 2) == 2);
}

TEST_CASE("std::find")
{
    sqlitemap sm;
    sm["k1"] = "x";
    sm["k2"] = "xx";
    sm["k3"] = "xxx";
    sm["k4"] = "xxxx";
    sm["k5"] = "xxxxx";

    std::pair<std::string, std::string> subject = {"k3", "xxx"};
    auto result = std::find(sm.begin(), sm.end(), subject);
    REQUIRE(result != sm.end());
    REQUIRE(result->first == "k3");
}

TEST_CASE("std::find_if")
{
    sqlitemap sm;
    sm["k1"] = "x";
    sm["k2"] = "xx";
    sm["k3"] = "xxx";
    sm["k4"] = "xxxx";
    sm["k5"] = "xxxxx";

    auto result = std::find_if(sm.begin(), sm.end(), [](auto kv) { return kv.second.size() >= 3; });
    REQUIRE(result != sm.end());
    REQUIRE(result->first == "k3");

    auto begin = sm.begin();
    auto end = begin + 2;
    auto result_not_matching =
        std::find_if(begin, end, [](auto kv) { return kv.second.size() >= 4; });
    REQUIRE(result_not_matching == end);
}

TEST_CASE("std::find_if_not")
{
    sqlitemap sm;
    sm["k1"] = "x";
    sm["k2"] = "xx";
    sm["k3"] = "xxx";
    sm["k4"] = "xxxx";
    sm["k5"] = "xxxxx";

    auto result_found_not_matching =
        std::find_if_not(sm.begin(), sm.end(), [](auto kv) { return kv.second.size() >= 3; });
    REQUIRE(result_found_not_matching != sm.end());
    REQUIRE(result_found_not_matching->first == "k1");

    auto begin = sm.begin();
    auto end = begin + 2;
    auto result_all_matching =
        std::find_if_not(begin, end, [](auto kv) { return kv.second.size() < 10; });
    REQUIRE(result_all_matching == end);
}

TEST_CASE("std::search")
{
#ifdef _WIN32
    WARN("std::search requires forward_iterator when using MSVC");
#else
    sqlitemap sm;
    sm["k1"] = "x";
    sm["k2"] = "xx";
    sm["k3"] = "xxx";
    sm["k4"] = "xxxx";

    std::vector<std::pair<std::string, std::string>> sub = {{"k2", "xx"}, {"k3", "xxx"}};
    auto it = std::search(sm.begin(), sm.end(), sub.begin(), sub.end());
    REQUIRE(it != sm.end());
    REQUIRE(it->first == "k2");

    std::vector<std::pair<std::string, std::string>> missing = {{"k5", "xxxxx"}};
    REQUIRE(std::search(sm.begin(), sm.end(), missing.begin(), missing.end()) == sm.end());
#endif
}

TEST_CASE("std::count")
{
    sqlitemap sm;
    sm["k1"] = "x";
    sm["k2"] = "xx";
    sm["k3"] = "xxx";
    sm["k4"] = "xxxx";
    sm["k5"] = "xxxxx";

    std::pair<std::string, std::string> existing = {"k3", "xxx"};
    REQUIRE(std::count(sm.begin(), sm.end(), existing) == 1);

    std::pair<std::string, std::string> missing = {"key", "missing"};
    REQUIRE(std::count(sm.begin(), sm.end(), missing) == 0);
}

TEST_CASE("std::count_if")
{
    sqlitemap sm;
    sm["k1"] = "x";
    sm["k2"] = "xx";
    sm["k3"] = "xxx";
    sm["k4"] = "xxxx";
    sm["k5"] = "xxxxx";

    auto result_founds =
        std::count_if(sm.begin(), sm.end(), [](auto kv) { return kv.second.size() >= 3; });
    REQUIRE(result_founds == 3);

    auto result_missing =
        std::count_if(sm.begin(), sm.end(), [](auto kv) { return kv.first.size() == 1; });
    REQUIRE(result_missing == 0);
}

TEST_CASE("std::adjacent_find")
{
    sqlitemap sm;
    sm["k1"] = "x";
    sm["k2"] = "xx";
    sm["k3"] = "xx";
    sm["k4"] = "xxx";

    auto adjacents_found = std::adjacent_find(sm.begin(), sm.end(), [](auto& a, auto& b)
                                              { return a.second == b.second; });
    REQUIRE(adjacents_found != sm.end());
    REQUIRE(adjacents_found->second == "xx");

    sqlitemap unique;
    unique["k1"] = "x";
    unique["k2"] = "xx";
    unique["k3"] = "xxx";

    auto no_adjacents = std::adjacent_find(unique.begin(), unique.end(),
                                           [](auto& a, auto& b) { return a.second == b.second; });
    REQUIRE(no_adjacents == unique.end());
}

TEST_CASE("std::none_of")
{
    sqlitemap sm;
    sm["k1"] = "x";
    sm["k2"] = "xx";
    sm["k3"] = "xxx";
    sm["k4"] = "xxxx";

    bool has_no_long_value =
        std::none_of(sm.begin(), sm.end(), [](const auto& kv) { return kv.second.size() > 5; });
    REQUIRE(has_no_long_value);

    sqlitemap sm_with_long;
    sm_with_long["k1"] = "x";
    sm_with_long["k2"] = "xxxxxx";

    has_no_long_value = std::none_of(sm_with_long.begin(), sm_with_long.end(),
                                     [](const auto& pair) { return pair.second.size() > 5; });
    REQUIRE_FALSE(has_no_long_value);
}

TEST_CASE("std::any_of")
{
    sqlitemap sm;
    sm["k1"] = "x";
    sm["k2"] = "xx";
    sm["k3"] = "xxx";
    sm["k4"] = "xxxx";

    bool has_xxx =
        std::any_of(sm.begin(), sm.end(), [](const auto& pair) { return pair.second == "xxx"; });
    REQUIRE(has_xxx);

    bool has_yyy =
        std::any_of(sm.begin(), sm.end(), [](const auto& pair) { return pair.second == "yyyy"; });
    REQUIRE_FALSE(has_yyy);
}

TEST_CASE("std::all_of")
{
    sqlitemap sm;
    sm["k1"] = "x";
    sm["k2"] = "xx";
    sm["k3"] = "xxx";

    bool has_no_empty =
        std::all_of(sm.begin(), sm.end(), [](const auto& pair) { return !pair.second.empty(); });
    REQUIRE(has_no_empty);

    sqlitemap sm_with_empty;
    sm_with_empty["k1"] = "";
    sm_with_empty["k2"] = "xx";

    has_no_empty = std::all_of(sm_with_empty.begin(), sm_with_empty.end(),
                               [](const auto& pair) { return !pair.second.empty(); });
    REQUIRE_FALSE(has_no_empty);
}

TEST_CASE("std::equal")
{
    sqlitemap sm1;
    sm1["k1"] = "x";
    sm1["k2"] = "xx";

    sqlitemap sm2;
    sm2["k1"] = "x";
    sm2["k2"] = "xx";

    REQUIRE(std::equal(sm1.begin(), sm1.end(), sm2.begin()));

    sqlitemap sm3;
    sm3["k1"] = "x";
    sm3["k2"] = "yy";
    REQUIRE_FALSE(std::equal(sm1.begin(), sm1.end(), sm3.begin()));
}

TEST_CASE("std::accumulate")
{
    sqlitemap sm;
    sm["k1"] = "x";
    sm["k2"] = "xx";
    sm["k3"] = "xxx";

    auto total_length =
        std::accumulate(sm.begin(), sm.end(), size_t{0},
                        [](size_t sum, const auto& pair) { return sum + pair.second.size(); });
    REQUIRE(total_length == 6);
}

TEST_CASE("std::vector")
{
    sqlitemap sm;
    sm["k1"] = "x";
    sm["k2"] = "xx";
    sm["k3"] = "xxx";

    using val_vec = std::vector<std::pair<std::string, std::string>>;
    val_vec values(sm.begin(), sm.end());
    val_vec expected = {{"k1", "x"}, {"k2", "xx"}, {"k3", "xxx"}};
    REQUIRE(values == expected);
}

TEST_CASE("std::map")
{
    sqlitemap sm;
    sm["k1"] = "x";
    sm["k2"] = "xx";
    sm["k3"] = "xxx";

    std::map map(sm.begin(), sm.end());
    std::map<std::string, std::string> expected = {{"k1", "x"}, {"k2", "xx"}, {"k3", "xxx"}};
    REQUIRE(map == expected);
}