// sqlitemap_tiles
// SPDX-FileCopyrightText: 2024-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <regex>
#include <string>

#include <bw/sqlitemap/sqlitemap.hpp>

namespace sm = bw::sqlitemap;

struct tile_location
{
    int zoom;
    int col;
    int row;
};

struct tile_bitmap
{
    std::array<bool, 4 * 4> bitmap = {};
    void print() const
    {
        for (int i = 0; i < 4 * 4; i++)
        {
            if (i % 4 == 0)
            {
                std::cout << " ";
            }
            std::cout << (bitmap[i] ? "▓▓" : "░░");
            if (i % 4 == 3)
            {
                std::cout << "\n";
            }
        }
    }
};

template <typename T> sm::blob to_blob(const T& data)
{
    sm::blob blob(sizeof(T));
    std::memcpy(blob.data(), &data, sizeof(data));
    return blob;
}

template <typename T> T from_blob(sm::blob blob)
{
    T t;
    std::memcpy(&t, blob.data(), sizeof(T));
    return t;
}

class tiles
{
    using key_type = tile_location;
    using value_type = tile_bitmap;
    using key_codec_t = sm::codecs::key_codec<key_type, sm::blob>;
    using value_codec_t = sm::codecs::value_codec<value_type, sm::blob>;
    using db = sm::sqlitemap<sm::codecs::codec_pair<key_codec_t, value_codec_t>>;

  public:
    tiles()
        : data(sm::config(key_codec_t{to_blob<key_type>, from_blob<key_type>},
                          value_codec_t{to_blob<value_type>, from_blob<value_type>})
                   .log_level(sm::log_level::debug))
    {
        data.set(tile_location{0, 0, 0}, tile_bitmap{1, 1, 0, 0, //
                                                     1, 1, 0, 0, //
                                                     0, 1, 1, 0, //
                                                     1, 0, 0, 1});

        data.set(tile_location{1, 0, 0}, tile_bitmap{1, 1, 1, 1, //
                                                     1, 0, 0, 1, //
                                                     1, 0, 0, 1, //
                                                     1, 1, 1, 1});

        data.set(tile_location{1, 1, 0}, tile_bitmap{0, 0, 0, 0, //
                                                     0, 1, 1, 0, //
                                                     0, 1, 1, 0, //
                                                     0, 0, 0, 0});

        data.set(tile_location{1, 0, 1}, tile_bitmap{0, 0, 1, 0, //
                                                     0, 1, 1, 1, //
                                                     1, 1, 1, 0, //
                                                     0, 1, 0, 0});

        data.set(tile_location{1, 1, 1}, tile_bitmap{0, 1, 0, 0, //
                                                     1, 1, 1, 0, //
                                                     0, 1, 1, 1, //
                                                     0, 0, 1, 0});
        data.commit();
    }

    void print()
    {
        for (const auto& e : data)
        {
            std::cout << "\nzoom: " << e.first.zoom << " col:" << e.first.col
                      << " row:" << e.first.row << "\n\n";

            e.second.print();

            std::cout << std::endl;
        }
    }

  private:
    db data;
};

int main()
{
    std::cout << "sqlitemap_tiles - Demo of using blob data for storing keys and values\n\n";

    tiles tiles;
    tiles.print();

    return 0;
}
