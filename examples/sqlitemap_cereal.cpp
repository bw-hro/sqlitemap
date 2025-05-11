// sqlitemap_cereal
// SPDX-FileCopyrightText: 2024-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <bw/sqlitemap/sqlitemap.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>

struct person
{
    std::string name;
    std::string city;
    int age = 0;
};

// make person available to cereal lib
template <class Archive> void serialize(Archive& archive, person& p)
{
    archive(p.name, p.city, p.age);
}

// encoding function that takes a person struct and converts it into a blob
bw::sqlitemap::blob person_to_blob(const person& p)
{
    std::ostringstream oss(std::ios::binary);
    {
        cereal::BinaryOutputArchive output_archive(oss);
        output_archive(p);
    }

    std::string str = oss.str();
    return {reinterpret_cast<const std::byte*>(str.data()),
            reinterpret_cast<const std::byte*>(str.data() + str.size())};
}

// decoding function that takes a blob and coverts it into a person struct
person person_from_blob(const bw::sqlitemap::blob& bytes)
{
    std::string str(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::istringstream iss(str, std::ios::binary);
    cereal::BinaryInputArchive input_archive(iss);
    person p;
    input_archive(p);
    return p;
}

int main()
{
    std::cout << "sqlitemap_cereal - Example of storing struct instances in a sqlitemap as blob\n";

    // create sqlitemap using custom codec for person values
    auto vc = bw::sqlitemap::value_codec(person_to_blob, person_from_blob);
    bw::sqlitemap::sqlitemap db(bw::sqlitemap::config(vc).filename("persons.db").table("persons"));

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
