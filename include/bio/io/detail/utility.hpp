// -----------------------------------------------------------------------------------------------------
// Copyright (c) 2006-2021, Knut Reinert & Freie Universität Berlin
// Copyright (c) 2016-2021, Knut Reinert & MPI für molekulare Genetik
// Copyright (c) 2020-2021, deCODE Genetics
// This file may be used, modified and/or redistributed under the terms of the 3-clause BSD-License
// shipped with this file and also available at: https://github.com/seqan/bio/blob/master/LICENSE.md
// -----------------------------------------------------------------------------------------------------

/*!\file
 * \brief Provides miscellaneous utilities.
 * \author Hannes Hauswedell <hannes.hauswedell AT decode.is>
 */

#pragma once

#include <algorithm>
#include <concepts>
#include <filesystem>
#include <ranges>
#include <string>

#include <bio/meta/type_list/function.hpp>
#include <bio/meta/type_list/type_list.hpp>
#include <bio/meta/type_traits/template_inspection.hpp>

#include <bio/io/exception.hpp>

namespace bio::io::detail
{

/*!\addtogroup io
 * \{
 */

//!\brief Can be included as a member to infer whether parent is in moved-from state.
//!\ingroup io
struct move_tracker
{
    //!\brief Defaulted.
    move_tracker() = default;

    //!\brief Sets moved-from state.
    move_tracker(move_tracker && rhs) { rhs.moved_from = true; }
    //!\brief Sets moved-from state.
    move_tracker & operator=(move_tracker && rhs)
    {
        rhs.moved_from = true;
        return *this;
    }

    move_tracker(move_tracker const & rhs)             = default; //!< Defaulted.
    move_tracker & operator=(move_tracker const & rhs) = default; //!< Defaulted.

    //!\brief The state.
    bool moved_from = false;
};

//!\brief Helper for clearing objects that provide such functionality.
//!\ingroup io
void clear(auto && arg) requires(requires { arg.clear(); })
{
    arg.clear();
}

//!\overload
void clear(auto && arg)
{
    arg = {};
}

} // namespace bio::io::detail
