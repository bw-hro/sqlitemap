[project_name]: sqlitemap

# sqlitemap — Persistent Map Backed by SQLite

[![CI Ubuntu](https://github.com/bw-hro/sqlitemap/actions/workflows/ubuntu.yml/badge.svg?branch=main)](https://github.com/bw-hro/sqlitemap/actions/workflows/ubuntu.yml)
[![CI Windows](https://github.com/bw-hro/sqlitemap/actions/workflows/windows.yml/badge.svg?branch=main)](https://github.com/bw-hro/sqlitemap/actions/workflows/windows.yml)
[![CI macOS](https://github.com/bw-hro/sqlitemap/actions/workflows/macos.yml/badge.svg?branch=main)](https://github.com/bw-hro/sqlitemap/actions/workflows/macos.yml)
[![code coverage](https://bw-hro.github.io/sqlitemap/coverage-report/badge.svg)](https://bw-hro.github.io/sqlitemap/coverage-report)
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/bw-hro/sqlitemap/main/LICENSE.txt)
[![GitHub Releases](https://img.shields.io/github/release/bw-hro/sqlitemap.svg)](https://github.com/bw-hro/sqlitemap/releases)
[![Vcpkg Version](https://img.shields.io/vcpkg/v/sqlitemap)](https://vcpkg.link/ports/sqlitemap)


**sqlitemap** is a lightweight C++ wrapper around SQLite that provides a simple, map-like interface.  
It’s designed to make working with key-value storage easy and convenient — similar to how [sqlitedict](https://github.com/piskvorky/sqlitedict) works in the Python world.

## Features

- Persistent key-value storage using SQLite
- Easy-to-use map-like interface in C++
- Minimal dependencies (just requires [SQLite](https://sqlite.org))

## Usage

### Write

```cpp
#include <bw/sqlitemap/sqlitemap.hpp>

int main()
{
    bw::sqlitemap::sqlitemap db("example.sqlite");

    db["a"] = "first-item";
    db["b"] = "second-item";
    db["c"] = "third-item";

    // Commit to save items
    db.commit();

    db["d"] = "yet-another-item";
    // Forgot to commit here, that item will never be saved.
    db.close();

    // Always remember to commit, or enable autocommit with
    //
    //     bw::sqlitemap::sqlitemap db(bw::sqlitemap::config()
    //        .file("example.sqlite")
    //        .auto_commit(true));
    //
    // Autocommit is off by default for performance.


    db.connect(); // reconnect, after close
    // some additional helpful write methods, among others:
    db.set("x", "draft");     // set or update value
    db.del("x");              // delete value
    db.emplace("y", "draft"); // insert value if not exists
    db.erase("y");            // delete value returns number 0 if not exists, 1 otherwise
    db.insert(std::pair{"z", "draft"}); // insert value, returns iterator to the inserted item
                                        // attention: iterator can not be advanced,
                                        // it can only be used to access the value
    db.clear(); // clear all items
}
```

### Read

```cpp
#include <bw/sqlitemap/sqlitemap.hpp>

int main()
{
    bw::sqlitemap::sqlitemap db("example.sqlite");
    std::cout << "There are " << db.size() << " items in the database" << std::endl;
    // There are 3 items in the database

    // Standard map like interface. operator[], iterator, ...

    for (auto& [key, value] : db)
    {
        std::cout << key << " = " << value << std::endl;
    }
    // a = first-item
    // b = second-item
    // c = third-item

    std::string value_a = db["a"];
    std::cout << "value of a: " << value_a << ", value of b: " << db["b"] << std::endl;
    // value of a: first-item, value of b: second-item

    // some additional helpful access methods, among others:
    db.get("a");      // returns value or throws sqlitemap_error if not found
    db.try_get("b");  // returns std::optional containing value or empty if not found
    db.find("c");     // returns iterator to the found item or end() if not found, attention: iterator
                      // can not be advanced, it can only be used to access the value
    db.contains("d"); // returns true if key is found, false otherwise
    db.count("e");    // returns number of items with the given key, 0 or 1
}
```
