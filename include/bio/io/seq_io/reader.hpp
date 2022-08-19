// -----------------------------------------------------------------------------------------------------
// Copyright (c) 2006-2021, Knut Reinert & Freie Universität Berlin
// Copyright (c) 2016-2021, Knut Reinert & MPI für molekulare Genetik
// Copyright (c) 2020-2021, deCODE Genetics
// This file may be used, modified and/or redistributed under the terms of the 3-clause BSD-License
// shipped with this file and also available at: https://github.com/seqan/seqan3/blob/master/LICENSE.md
// -----------------------------------------------------------------------------------------------------

/*!\file
 * \brief Provides bio::io::seq_io::reader.
 * \author Hannes Hauswedell <hannes.hauswedell AT decode.is>
 */

#pragma once

#include <string>
#include <vector>

#include <bio/alphabet/aminoacid/aa27.hpp>
#include <bio/alphabet/concept.hpp>
#include <bio/alphabet/nucleotide/dna5.hpp>
#include <bio/alphabet/quality/phred63.hpp>
#include <bio/io/misc.hpp>
#include <bio/ranges/views/char_strictly_to.hpp>

#include <bio/io/detail/reader_base.hpp>
#include <bio/io/format/fasta_input_handler.hpp>
#include <bio/io/format/fastq_input_handler.hpp>
#include <bio/io/seq_io/reader_options.hpp>

namespace bio::io::seq_io
{

// ----------------------------------------------------------------------------
// reader
// ----------------------------------------------------------------------------

/*!\brief A class for reading sequence files, e.g. FASTA, FASTQ.
 * \ingroup seq_io
 *
 * \details
 *
 * ### Introduction
 *
 * Sequence files are the most generic and common biological files. Well-known formats include
 * FastA and FastQ, but sometimes you may also be interested in treating SAM or BAM files as sequence
 * files, discarding the alignment.
 *
 * The Sequence I/O reader supports reading the following fields:
 *
 *   1. bio::io::field::seq
 *   2. bio::io::field::id
 *   3. bio::io::field::qual
 *
 * And it supports the following formats:
 *
 *   1. FastA (see also bio::io::fasta)
 *   2. FastQ (see also bio::io::fastq)
 *
 * Fields that are not present in a format (e.g. bio::io::field::qual in FastA) will be returned empty.
 *
 * ### Simple usage
 *
 * Iterate over a sequence file via the reader and print the record's contents:
 *
 * \snippet test/snippet/seq_io/seq_io_reader.cpp simple_usage_file
 *
 * Read from standard input instead of a file:
 *
 * \snippet test/snippet/seq_io/seq_io_reader.cpp simple_usage_stream
 *
 * ### Decomposed iteration
 *
 * The records can be decomposed on-the-fly using
 * [structured bindings](https://en.cppreference.com/w/cpp/language/structured_binding):
 *
 * \snippet test/snippet/seq_io/seq_io_reader.cpp decomposed
 *
 * Note that the order of the fields is defined by bio::io::seq_io::default_field_ids and independent
 * of the names you give to the bindings.
 *
 * ### Views on files
 *
 * This iterates over the first five records where the sequence length is at least 10:
 *
 * \snippet test/snippet/seq_io/seq_io_reader.cpp views
 *
 * ### Specifying options
 *
 * This snippet demonstrates how to read sequence data as protein data and have the IDs truncated
 * at the first whitespace:
 * \snippet test/snippet/seq_io/seq_io_reader.cpp options
 *
 * If you need to modify or store the records, request *deep records* from the reader:
 * \snippet test/snippet/seq_io/seq_io_reader.cpp options2
 *
 * For more information on *shallow* vs *deep*, see \ref shallow_vs_deep
 *
 * For more advanced options, see bio::io::seq_io::reader_options.
 */
template <typename... option_args_t>
class reader : public reader_base<reader<option_args_t...>, reader_options<option_args_t...>>
{
private:
    /*!\name CRTP related entities
     * \{
     */
    //!\brief The base class.
    using base_t = reader_base<reader<option_args_t...>, reader_options<option_args_t...>>;
    //!\brief Befriend CRTP-base.
    friend base_t;
    //!\cond
    // Doxygen is confused by this for some reason
    friend detail::in_file_iterator<reader>;
    //!\endcond

    //!\brief Expose the options type to the base-class.
    using options_t = reader_options<option_args_t...>;
    //!\}

public:
    //!\brief Inherit the format_type definition.
    using format_type = typename base_t::format_type;

    // clang-format off
    //!\copydoc bio::io::reader_base::reader_base(std::filesystem::path const & filename, format_type const & fmt, options_t const & opt = options_t{})
    // clang-format on
    reader(std::filesystem::path const &            filename,
           format_type const &                      fmt,
           reader_options<option_args_t...> const & opt = reader_options<option_args_t...>{}) :
      base_t{filename, fmt, opt}
    {}

    //!\overload
    explicit reader(std::filesystem::path const &            filename,
                    reader_options<option_args_t...> const & opt = reader_options<option_args_t...>{}) :
      base_t{filename, opt}
    {}

    // clang-format off
    //!\copydoc bio::io::reader_base::reader_base(std::istream & str, format_type const & fmt, options_t const & opt = options_t{})
    // clang-format on
    reader(std::istream &                           str,
           format_type const &                      fmt,
           reader_options<option_args_t...> const & opt = reader_options<option_args_t...>{}) :
      base_t{str, fmt, opt}
    {}

    //!\overload
    template <movable_istream temporary_stream_t>
        //!\cond REQ
        requires(!std::is_lvalue_reference_v<temporary_stream_t>)
    //!\endcond
    reader(temporary_stream_t &&                    str,
           format_type const &                      fmt,
           reader_options<option_args_t...> const & opt = reader_options<option_args_t...>{}) :
      base_t{std::move(str), fmt, opt}
    {}
};

} // namespace bio::io::seq_io
