// sqlitemap_zlib
// SPDX-FileCopyrightText: 2024-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <bw/sqlitemap/sqlitemap.hpp>
#include <zlib.h>

constexpr char lorem_ipsum[] =
    "Lorem ipsum\n"
    "\n"
    "Lorem ipsum dolor sit amet, consectetur adipisici elit, sed eiusmod tempor incidunt ut\n"
    "labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco\n"
    "laboris nisi ut aliquid ex ea commodi consequat. Quis aute iure reprehenderit in\n"
    "voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint obcaecat\n"
    "cupiditat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.\n"
    "\n"
    "Duis autem vel eum iriure dolor in hendrerit in vulputate velit esse molestie consequat,\n"
    "vel illum dolore eu feugiat nulla facilisis at vero eros et accumsan et iusto odio\n"
    "dignissim qui blandit praesent luptatum zzril delenit augue duis dolore te feugait nulla\n"
    "facilisi. Lorem ipsum dolor sit amet, consectetuer adipiscing elit, sed diam nonummy nibh\n"
    "euismod tincidunt ut laoreet dolore magna aliquam erat volutpat.\n"
    "\n"
    "Ut wisi enim ad minim veniam, quis nostrud exerci tation ullamcorper suscipit lobortis\n"
    "nisl ut aliquip ex ea commodo consequat. Duis autem vel eum iriure dolor in hendrerit in\n"
    "vulputate velit esse molestie consequat, vel illum dolore eu feugiat nulla facilisis at\n"
    "vero eros et accumsan et iusto odio dignissim qui blandit praesent luptatum zzril delenit\n"
    "augue duis dolore te feugait nulla facilisi.\n";

bw::sqlitemap::blob compress_doc(const std::string& input)
{
    // store the original size as 4byte int before compressed payload

    auto original_size = static_cast<uint32_t>(input.size());
    auto compressed_size = compressBound(input.size());
    bw::sqlitemap::blob compressed(sizeof(uint32_t) + compressed_size);
    std::memcpy(compressed.data(), &original_size, sizeof(uint32_t));

    int res =
        compress(reinterpret_cast<Bytef*>(compressed.data() + sizeof(uint32_t)), &compressed_size,
                 reinterpret_cast<const Bytef*>(input.data()), original_size);

    if (res != Z_OK)
    {
        throw std::runtime_error("Compression failed");
    }

    compressed.resize(sizeof(uint32_t) + compressed_size);
    return compressed;
}

std::string decompress_doc(const bw::sqlitemap::blob& compressed)
{
    // read the original size from first 4bytes of compressed data

    if (compressed.size() < sizeof(uint32_t))
    {
        throw std::runtime_error("Input too small to contain size");
    }

    uLongf original_size = 0;
    std::memcpy(&original_size, compressed.data(), sizeof(uint32_t));

    std::string decompressed(original_size, '\0');
    int res = uncompress(reinterpret_cast<Bytef*>(decompressed.data()), &original_size,
                         reinterpret_cast<const Bytef*>(compressed.data() + sizeof(uint32_t)),
                         compressed.size() - sizeof(uint32_t));
    if (res != Z_OK)
    {
        throw std::runtime_error("Decompression failed");
    }

    return decompressed;
}

int main()
{
    std::cout << "sqlitemap_zlib - Example of storing zlib compressed documents in a sqlitemap\n";

    auto vc = bw::sqlitemap::value_codec(compress_doc, decompress_doc);
    bw::sqlitemap::sqlitemap db(bw::sqlitemap::config(vc).filename("documents.db"));

    db["lorem_ipsum"] = lorem_ipsum;
    db.commit();

    std::cout << "\nDocument 'lorem_ipsum'\n\n" << db["lorem_ipsum"] << "\n";
    return 0;
}
