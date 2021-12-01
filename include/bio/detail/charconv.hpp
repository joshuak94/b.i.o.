// -----------------------------------------------------------------------------------------------------
// Copyright (c) 2006-2021, Knut Reinert & Freie Universität Berlin
// Copyright (c) 2016-2021, Knut Reinert & MPI für molekulare Genetik
// Copyright (c) 2020-2021, deCODE Genetics
// This file may be used, modified and/or redistributed under the terms of the 3-clause BSD-License
// shipped with this file and also available at: https://github.com/seqan/bio/blob/master/LICENSE.md
// -----------------------------------------------------------------------------------------------------

/*!\file
 * \brief Provides exception types.
 * \author Hannes Hauswedell <hannes.hauswedell AT decode.is>
 */

#pragma once

#include <cassert>
#include <charconv>
#include <concepts>
#include <ranges>
#include <string_view>

#include <seqan3/utility/concept/exposition_only/core_language.hpp>

#include <bio/platform.hpp>

namespace bio::detail
{

/*!\addtogroup bio
 * \{
 */

//!\brief Wrapper around standard library std::to_chars with fallback for floats on GCC10.
std::to_chars_result to_chars(char * first, char * last, auto in)
{
#if defined(__cpp_lib_to_chars) && (__cpp_lib_to_chars >= 201611)
    constexpr static bool float_support = true;
#else
    constexpr static bool float_support = false;
#endif

    if constexpr (std::integral<decltype(in)> || float_support)
    {
        return std::to_chars(first, last, in);
    }
    else // very hacky version
    {
        int count = std::sprintf(first, "%f", in);
        if (count < 0)
            return {.ptr = nullptr, .ec = std::errc::io_error};

        assert(first + count <= last);

        if (std::string_view{first, static_cast<size_t>(count)}.find('.') != std::string_view::npos)
            while (count > 0 && first[count - 1] == '0')
                --count;
        return {.ptr = first + count, .ec{}};
    }
}

// TODO write append_number_to_string

/*!\brief Turn a string into a number.
 * \param[in] input The input string.
 * \param[out] number The variable holding the result.
 * \throws std::runtime_error If there was an error during conversion.
 * \details
 *
 * Relies on std::from_chars to efficiently convert but accepts std::string_view and throws on error so
 * there is no return value that needs to be checked.
 */
void string_to_number(std::string_view const input, seqan3::arithmetic auto & number)
{
    std::from_chars_result res = std::from_chars(input.data(), input.data() + input.size(), number);
    if (res.ec != std::errc{} || res.ptr != input.data() + input.size())
        throw std::runtime_error{std::string{"Could not convert \""} + std::string{input} + "\" into a number."};
}

/*!\brief Convert something to a string.
 * \details
 *
 * ### Attention
 *
 * This function is not efficient. Do not use it in performance-critical code.
 */
std::string to_string(auto && in)
{
    using in_t = std::remove_cvref_t<decltype(in)>;
    if constexpr (seqan3::arithmetic<in_t>)
        return std::to_string(in);
    else if constexpr (std::same_as<in_t, std::string>)
        return in;
    else if constexpr (std::constructible_from<std::string, in_t>)
        return std::string{in};
    else
        static_assert(std::constructible_from<std::string, in_t>, "Type cannot be converted to string");
}

//!\}

} // namespace bio::detail
