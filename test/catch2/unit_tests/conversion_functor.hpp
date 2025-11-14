// sqlitemap
// SPDX-FileCopyrightText: 2024-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <iostream>
#include <string>

namespace bw::testhelper
{

struct counts
{
    int default_ctor_count = 0;
    int copy_ctor_count = 0;
    int move_ctor_count = 0;
    int copy_assign_count = 0;
    int move_assign_count = 0;
    int dtor_count = 0;
};

template <typename Derived> struct instance_counter
{
    std::string instance_name;
    counts* counts_ptr;
    bool was_moved = false;

    instance_counter(const std::string& name = "Instance", counts* c_ptr = nullptr)
        : instance_name(name)
        , counts_ptr(c_ptr)
    {
        if (!counts_ptr)
        {
            static counts default__global_counts;
            counts_ptr = &default__global_counts;
        }
        ++counts_ptr->default_ctor_count;
        std::cout << instance_name << " constructed count:" << counts_ptr->default_ctor_count
                  << "\n";
    }

    instance_counter(const instance_counter& other)
        : instance_name(other.instance_name)
        , counts_ptr(other.counts_ptr)
    {
        if (counts_ptr)
            ++counts_ptr->copy_ctor_count;
        std::cout << instance_name << " copy constructed count:" << counts_ptr->copy_ctor_count
                  << "\n";
    }

    instance_counter(instance_counter&& other) noexcept
        : instance_name(std::move(other.instance_name))
        , counts_ptr(other.counts_ptr)
    {
        if (counts_ptr)
            ++counts_ptr->move_ctor_count;
        std::cout << instance_name << " move constructed count:" << counts_ptr->move_ctor_count
                  << "\n";
        was_moved = true;
    }

    instance_counter& operator=(const instance_counter& other)
    {
        instance_name = other.instance_name;
        counts_ptr = other.counts_ptr;
        if (counts_ptr)
            ++counts_ptr->copy_assign_count;
        std::cout << instance_name << " copy assigned count:" << counts_ptr->copy_assign_count
                  << "\n";
        return static_cast<Derived&>(*this);
    }

    instance_counter& operator=(instance_counter&& other) noexcept
    {
        instance_name = std::move(other.instance_name);
        counts_ptr = other.counts_ptr;
        if (counts_ptr)
            ++counts_ptr->move_assign_count;
        std::cout << instance_name << " move assigned count:" << counts_ptr->move_assign_count
                  << "\n";

        was_moved = true;

        return static_cast<Derived&>(*this);
    }

    ~instance_counter()
    {
        if (was_moved)
            return; // avoid counting destructors for moved-from instances

        if (counts_ptr)
            ++counts_ptr->dtor_count;

        std::cout << instance_name << " destroyed count:" << counts_ptr->dtor_count << "\n";
    }
};

// --- Conversion Functor with per-instance tracking and name ---
template <typename IN_T, typename OUT_T>
struct conversion_functor : public instance_counter<conversion_functor<IN_T, OUT_T>>
{
    using counter = instance_counter<conversion_functor<IN_T, OUT_T>>;

    explicit conversion_functor(const std::string& name,
                                std::function<OUT_T(const IN_T&)>&& convert,
                                counts* counts_ptr = nullptr)
        : counter(name, counts_ptr)
        , name(name.empty() ? "ConversionFunctor" : name)
        , convert(std::move(convert))
    {
    }

    OUT_T operator()(const IN_T& value) const
    {
        auto result = convert(value);
        std::cout << name << " converted " << value << " to " << result << "\n";
        return result;
    }

    std::string name;
    std::function<OUT_T(const IN_T&)> convert;
};

} // namespace bw::testhelper