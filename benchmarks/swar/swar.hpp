// Copyright (C) 2020-2022 Jonathan Müller and lexy contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef BENCHMARKS_SWAR_SWAR_HPP_INCLUDED
#define BENCHMARKS_SWAR_SWAR_HPP_INCLUDED

#include <lexy/input/buffer.hpp>
#include <nanobench.h>

#if defined(__GNUC__)
#    define LEXY_NOINLINE [[gnu::noinline]]
#else
#    define LEXY_NOINLINE
#endif

template <typename Encoding>
class swar_disabled_reader
{
public:
    using encoding = Encoding;
    using iterator = const typename Encoding::char_type*;

    explicit swar_disabled_reader(iterator begin) noexcept : _cur(begin) {}

    auto peek() const noexcept
    {
        // The last one will be EOF.
        return *_cur;
    }

    void bump() noexcept
    {
        ++_cur;
    }

    iterator position() const noexcept
    {
        return _cur;
    }

    void set_position(iterator new_pos) noexcept
    {
        _cur = new_pos;
    }

private:
    iterator _cur;
};

template <typename Encoding>
constexpr auto disable_swar(lexy::_br<Encoding> reader)
{
    return swar_disabled_reader<Encoding>(reader.position());
}

lexy::buffer<lexy::utf8_encoding> random_buffer(std::size_t size, float unicode_ratio);

#endif // BENCHMARKS_SWAR_SWAR_HPP_INCLUDED

