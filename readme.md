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
