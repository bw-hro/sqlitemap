// sqlitemap_json
// SPDX-FileCopyrightText: 2024-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <bw/sqlitemap/sqlitemap.hpp>
#include <nlohmann/json.hpp>

using namespace bw::sqlitemap;
using namespace nlohmann;

struct person
{
    std::string name;
    std::string city;
    int age = 0;
};

// encoding function that takes a person struct and converts it into a JSON string
std::string person_to_json(const person& p)
{
    json j{{"name", p.name}, {"age", p.age}, {"city", p.city}};
    return j.dump();
}

// decoding function that takes a JSON string and coverts it into a person struct
person person_from_json(const std::string& json_str)
{
    person p;
    json j = json::parse(json_str);
    j.at("name").get_to(p.name);
    j.at("age").get_to(p.age);
    j.at("city").get_to(p.city);
    return p;
}

int main()
{
    std::cout << "sqlitemap_json - Example of storing struct instances in a sqlitemap as JSON\n";

    // create sqlitemap using custom codec for person values
    auto vc = value_codec(person_to_json, person_from_json);
    sqlitemap db(config(vc).filename("data.sqlite").table("persons"));

    // storing some persons
    db["1"] = {"Homer", "Springfield", 34};
    db["2"] = {"Bart", "Springfield", 10};
    db["3"] = {"Lisa", "Springfield", 8};
    db["4"] = {"Shelby", "Shelbyville", 40};
    db.commit();

    // query a person by key "2"
    person p = db["2"];

    std::cout << "found " << p.name << " (" << p.age << " years) from " << p.city << "\n";

    return 0;
}
