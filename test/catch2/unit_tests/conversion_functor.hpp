// sqlitemap
// SPDX-FileCopyrightText: 2024-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <iostream>
#include <string>

namespace bw::testhelper
{

template <typename IN_T, typename OUT_T> struct conversion_functor
{
    explicit conversion_functor(const std::string& name,
                                std::function<OUT_T(const IN_T&)>&& convert)
        : name(name)
        , convert(std::move(convert))
    {
        std::cout << name << " constructed\n";
    }

    conversion_functor(const conversion_functor& other)
        : name(other.name)
        , convert(other.convert)
    {
        std::cout << name << " copy-constructed\n";
    }

    conversion_functor(conversion_functor&& other) noexcept
        : name(std::move(other.name))
        , convert(std::move(other.convert))
    {
        std::cout << name << " move-constructed\n";
    }

    conversion_functor& operator=(const conversion_functor& other)
    {
        name = other.name;
        convert = other.convert;
        std::cout << name << " copy-assigned\n";
        return *this;
    }

    conversion_functor& operator=(conversion_functor&& other) noexcept
    {
        name = std::move(other.name);
        convert = std::move(other.convert);
        std::cout << name << " move-assigned\n";
        return *this;
    }

    ~conversion_functor()
    {
        std::cout << name << " destroyed\n";
    }

    OUT_T operator()(const IN_T& value) const
    {
        auto converted = convert(value);
        std::cout << name << " converted " << value << " to " << converted << "\n";
        return converted;
    }

    std::string name;
    std::function<OUT_T(const IN_T&)> convert;
};

} // namespace bw::testhelper