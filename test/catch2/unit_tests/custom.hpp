// sqlitemap
// SPDX-FileCopyrightText: 2024-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <sstream>
#include <string>

namespace bw::testhelper
{

struct custom
{
    int counter;
};

struct point
{
    int x, y, z;

    bool operator==(const point& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }

    static std::string to_string(const point& p)
    {
        std::stringstream ss;
        ss << "{\"x\": " << p.x << ", \"y\": " << p.y << ", \"z\": " << p.z << "}";
        return ss.str();
    }

    static point from_string(const std::string& s)
    {
        point p;
        size_t start_pos, end_pos;

        start_pos = s.find("\"x\": ") + 5;
        end_pos = s.find(",", start_pos);
        p.x = std::stoi(s.substr(start_pos, end_pos - start_pos));

        start_pos = s.find("\"y\": ") + 5;
        end_pos = s.find(",", start_pos);
        p.y = std::stoi(s.substr(start_pos, end_pos - start_pos));

        start_pos = s.find("\"z\": ") + 5;
        end_pos = s.find("}", start_pos);
        p.z = std::stoi(s.substr(start_pos, end_pos - start_pos));

        return p;
    }
};

struct feature
{
    std::string title;
    int rating;

    bool operator==(const feature& other) const
    {
        return title == other.title && rating == other.rating;
    }

    static std::string to_string(const feature& f)
    {
        std::stringstream ss;
        ss << "{\"title\": \"" << f.title << "\", \"rating\": " << f.rating << "}";
        return ss.str();
    }

    static feature from_string(const std::string& s)
    {
        feature f;
        size_t start_pos, end_pos;

        start_pos = s.find("\"title\": \"") + 10;
        end_pos = s.find("\"", start_pos);
        f.title = s.substr(start_pos, end_pos - start_pos);

        start_pos = s.find("\"rating\": ") + 10;
        end_pos = s.find("}", start_pos);
        f.rating = std::stoi(s.substr(start_pos, end_pos - start_pos));

        return f;
    }
};

} // namespace bw::testhelper

namespace bw::sqlitemap::codecs
{

inline std::string to_string(const bw::testhelper::custom& input)
{
    return "custom[" + std::to_string(input.counter) + "]";
}

} // namespace bw::sqlitemap::codecs