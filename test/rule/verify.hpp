// Copyright (C) 2020 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#ifndef TEST_RULE_VERIFY_HPP_INCLUDED
#define TEST_RULE_VERIFY_HPP_INCLUDED

#include <cassert>
#include <doctest.h>
#include <lexy/dsl/base.hpp>
#include <lexy/dsl/literal.hpp>
#include <lexy/result.hpp>

#include "../test_encoding.hpp"

template <typename Error>
using test_error = typename Error::template error<test_input>;

template <typename Callback, typename Production = void>
struct test_context
{
    const char* str;

    using result_type = lexy::result<int, int>;

    template <typename SubProduction>
    constexpr auto sub_context()
    {
        return test_context<Callback, SubProduction>{str};
    }

    constexpr auto list_builder()
    {
        return Callback{str}.list();
    }

    template <typename Error>
    constexpr auto error(const test_input&, Error&& error) &&
    {
        if constexpr (std::is_same_v<Production, void>)
        {
            auto code = Callback{str}.error(LEXY_FWD(error));
            return result_type(lexy::result_error, code);
        }
        else
        {
            auto code = Callback{str}.error(Production{}, LEXY_FWD(error));
            return result_type(lexy::result_error, code);
        }
    }

    template <typename... Args>
    constexpr auto value(Args&&... args) &&
    {
        if constexpr (std::is_same_v<Production, void>)
        {
            auto code = Callback{str}.success(LEXY_FWD(args)...);
            return result_type(lexy::result_value, code);
        }
        else
        {
            auto code = Callback{str}.success(Production{}, LEXY_FWD(args)...);
            return result_type(lexy::result_value, code);
        }
    }
};

struct test_final_parser
{
    template <typename Context, typename Input, typename... Args>
    LEXY_DSL_FUNC auto parse(Context& context, Input& input, Args&&... args) ->
        typename Context::result_type
    {
        // We sneak in the final input position.
        return LEXY_MOV(context).value(input.cur(), LEXY_FWD(args)...);
    }
};

template <typename Callback, typename Rule>
constexpr int rule_matches(Rule, const char* str)
{
    auto input = lexy::zstring_input<test_encoding>(str);

    test_context<Callback> context{str};
    auto                   result = Rule::template parser<test_final_parser>::parse(context, input);
    if (result)
        return result.value();
    else
        return result.error();
}

#endif // TEST_RULE_VERIFY_HPP_INCLUDED

