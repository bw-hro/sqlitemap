# sqlitemap — Persistent Map Backed by SQLite

**sqlitemap** is a lightweight C++ wrapper around SQLite that provides a simple, map-like interface.  
It’s designed to make working with key-value storage easy and convenient — similar to how [sqlitedict](https://github.com/piskvorky/sqlitedict) works in the Python world.

## Features

- Persistent key-value storage using SQLite
- Easy-to-use map-like interface in C++
- Minimal dependencies (just requires [SQLite](https://sqlite.org))

## Example Usage

```cpp
#include <sqlitemap.hpp>

int main()
{
    bw::sqlitemap::sqlitemap db("example.db");

    db["hello"] = "world";
    std::string value = db["hello"];

    std::cout << value << std::endl;
}
