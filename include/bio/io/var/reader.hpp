// -----------------------------------------------------------------------------------------------------
// Copyright (c) 2006-2021, Knut Reinert & Freie Universität Berlin
// Copyright (c) 2016-2021, Knut Reinert & MPI für molekulare Genetik
// Copyright (c) 2020-2021, deCODE Genetics
// This file may be used, modified and/or redistributed under the terms of the 3-clause BSD-License
// shipped with this file and also available at: https://github.com/seqan/seqan3/blob/master/LICENSE.md
// -----------------------------------------------------------------------------------------------------

/*!\file
 * \brief Provides bio::io::var::reader.
 * \author Hannes Hauswedell <hannes.hauswedell AT decode.is>
 */

#pragma once

#include <filesystem>

#include <bio/meta/tag/vtag.hpp>

#include <bio/io/detail/index_tabix.hpp>
#include <bio/io/detail/reader_base.hpp>
#include <bio/io/format/bcf_input_handler.hpp>
#include <bio/io/format/vcf_input_handler.hpp>
#include <bio/io/var/header.hpp>
#include <bio/io/var/reader_options.hpp>

namespace bio::io::var
{

// ----------------------------------------------------------------------------
// reader
// ----------------------------------------------------------------------------

/*!\brief A class for reading variant files, e.g. VCF, BCF, GVCF.
 * \tparam option_args_t Arguments that are forwarded to bio::io::var::reader_options.
 * \ingroup var
 *
 * \details
 *
 * ### Introduction
 *
 * Variant files are files that contain sequence variation information. This reader supports the
 * following formats:
 *
 *   1. VCF (see also bio::io::vcf)
 *   2. BCF (see also bio::io::bcf)
 *
 * The Variant I/O reader creates records (bio::io::var::record) that each contain the well-known
 * members (chrom, pos, ID, ...).
 * The types and values in the record correspond to VCF specification by default (i.e. 1-based
 * positions, string as strings and not as numbers) **with one exception:** the genotypes are not
 * grouped by sample (as in the VCF format) but by genotype field (as in the BCF format).
 * This results in a notably better performance when reading BCF files.
 * The types of the record members can be changed to achieve different behaviour; see
 * bio::io::var::record for more details.
 *
 * If you only need to read VCF (and not BCF), and you do not want to parse the fields into high-level data structures
 * (and simply use them as strings), you can use bio::io::txt::reader instead of this reader.
 *
 * ### Simple usage
 *
 * Iterate over a variant file via the reader and print "CHROM:POS:REF:ALT" for each record:
 *
 * \snippet test/snippet/var/var_reader.cpp simple_usage_file
 *
 * Read from standard input instead of a file:
 *
 * \snippet test/snippet/var/var_reader.cpp simple_usage_stream
 *
 * ### Accessing more complex fields
 *
 * TODO
 *
 *
 * ### Region filtering
 *
 * Configure the reader to only show records in the region "20:17000-1230300":
 *
 * \snippet test/snippet/var/var_reader.cpp region
 *
 * This region filter requires an index, although region filtering without an index
 * is also available; see bio::io::var::reader_options::region.
 *
 * ### Views on readers
 *
 * Print information for the first five records where quality is better than 23:
 *
 * \snippet test/snippet/var/var_reader.cpp views
 *
 * ### Specifying options
 *
 * TODO
 *
 * For more advanced options, see bio::io::var::reader_options.
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
    friend io::detail::in_file_iterator<reader>;
    //!\endcond

    //!\brief Expose the options type to the base-class.
    using options_t = reader_options<option_args_t...>;
    //!\}

    /*!\name Member data
     * \{
     */
    //!\cond
    using base_t::at_end;
    using base_t::format;
    using base_t::format_handler;
    using base_t::options;
    using base_t::record_buffer;
    using base_t::stream;
    //!\endcond

    //!\brief A pointer to the header inside the format.
    var::header const * header_ptr = nullptr;
    //!\}

    //!\brief Jump to the region specified in the options.
    void jump_to_region()
    {
        std::filesystem::path index_file = options.region_index_file;

        // no index file was specified, try ".tbi"
        if (std::filesystem::path index_file_tmp = stream.filename();
            index_file.empty() && !index_file_tmp.empty() && std::filesystem::exists(index_file_tmp += ".tbi"))
        {
            index_file = index_file_tmp;
        }

        if (!index_file.empty())
        {
            io::detail::tabix_index index;
            index.read(index_file);
            std::vector<std::pair<uint64_t, uint64_t>> chunks = index.reg2chunks(options.region);

            /* IMPLEMENTATION NOTE
             * We are currently doing a simplified indexed access where we do not process all
             * possible chunks but just do a linear scan from the beginning of the first overlapping chunk.
             * This is definitely worse than what htslib is doing, but still very good for the tests performed.
             * I doubt that we can ever reach htslib's performance fully while using C++ iostreams.
             */

            // we take the smallest begin-offset
            uint64_t const min_beg           = std::ranges::min(chunks | std::views::elements<0>);
            auto [disk_offset, block_offset] = io::detail::decode_bgz_virtual_offset(min_beg);

            // seek on-disk
            stream.seekg_primary(disk_offset);
            // seek inside block
            io::detail::fast_istreambuf_iterator<char> it{stream};
            it.skip_n(block_offset);
            std::visit([](auto & f) { f.reset_stream(); }, format_handler);
        }
        else if (!options.region_index_optional)
        {
            throw bio::io::file_open_error{
              "No index file was found. To allow linear-time filtering without an index, "
              "set options.region_index_optional to true."};
        }
    }

    //!\brief Initialise the format handler and read first record.
    void init()
    {
        /* set format-handler */
        std::visit([&](auto f) { format_handler = format_input_handler<decltype(f)>{stream, options}; }, format);

        /* region filtering */
        if (!options.region.chrom.empty())
            jump_to_region();

        /* read first record */
        read_next_record();
    }
    //!\brief Tell the format to move to the next record and update the buffer.
    void read_next_record()
    {
        if (at_end)
            return;

        // at end if we could not read further
        if (std::istreambuf_iterator<char>{stream} == std::istreambuf_iterator<char>{})
        {
            at_end = true;
            return;
        }

        assert(!format_handler.valueless_by_exception());

        /* regular, unrestricted reading */
        if (options.region.chrom.empty())
        {
            std::visit([&](auto & f) { f.parse_next_record_into(record_buffer); }, format_handler);
        }
        else /* only read sub-region */
        {
            // this record holds the bare minimum to check if regions overlap; always shallow
            using record_t = record<std::string_view,
                                    int64_t,
                                    meta::ignore_t,
                                    std::string_view,
                                    meta::ignore_t,
                                    meta::ignore_t,
                                    meta::ignore_t,
                                    meta::ignore_t,
                                    meta::ignore_t>;
            record_t temp_record;

            while (true)
            {
                // at end if we could not read further
                if (std::istreambuf_iterator<char>{stream} == std::istreambuf_iterator<char>{})
                {
                    at_end = true;
                    break;
                }

                std::visit([&](auto & f) { f.parse_next_record_into(temp_record); }, format_handler);

                // TODO undo "- 1" if interval notation gets decided on
                std::weak_ordering ordering =
                  genomic_region::relative_to(temp_record.chrom,
                                              temp_record.pos - 1,
                                              temp_record.pos - 1 + (int64_t)temp_record.ref.size(),
                                              options.region.chrom,
                                              options.region.beg,
                                              options.region.end);
                if (ordering == std::weak_ordering::less) // records lies before the target region → skip
                {
                    continue;
                }
                else if (ordering == std::weak_ordering::equivalent) // records overlaps target region → take it
                {
                    // full parsing of the same record
                    std::visit([&](auto & f) { f.parse_current_record_into(record_buffer); }, format_handler);
                    break;
                }
                else if (ordering == std::weak_ordering::greater) // record begins after target region start → at end
                {
                    at_end = true;
                    break;
                }
            }
        }
    }

public:
    //!\brief Inherit the format_type definition.
    using format_type = typename base_t::format_type;

    /*!\brief Construct from filename.
     * \param[in] filename  Path to the file you wish to open.
     * \param[in] fmt       The file format given as e.g. `vcf{}` [optional]
     * \param[in] opt       Reader options (bio::io::var::reader_options). [optional]
     * \throws bio::io::file_open_error If the file could not be opened, e.g. non-existant, non-readable, unknown
     * format.
     *
     * \details
     *
     * In addition to the file name, you may fix the format and/or provide options.
     *
     * ### Decompression
     *
     * This constructor transparently applies a decompression stream on top of the file stream in case
     * the file is detected as being compressed.
     * See the section on compression and decompression (TODO) for more information.
     */
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

    /*!\brief Construct from an existing stream and with specified format.
     * \param[in] str  The stream to operate on.
     * \param[in] fmt  The file format given as e.g. `vcf{}`. [required]
     * \param[in] opt  Reader options (bio::io::var::reader_options). [optional]
     *
     * \details
     *
     * In addition to the stream, you must fix the format and you may optionally provide options.
     *
     * ### Decompression
     *
     * This constructor transparently applies a decompression stream on top of the stream in case
     * it is detected as being compressed.
     * See the section on compression and decompression (TODO) for more information.
     */
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

    //!\brief Access the header.
    bio::io::var::header const & header()
    {
        if (header_ptr == nullptr)
        {
            // ensure that the format_handler is created
            this->begin();

            header_ptr = std::visit([](auto const & handler) { return &handler.get_header(); }, format_handler);
        }

        return *header_ptr;
    }

    using base_t::reopen;

    /*!\brief Re-create this reader on the specified region.
     * \details
     *
     * Note that the header is not parsed again.
     */
    void reopen(genomic_region const & region)
    {
        at_end         = false;
        options.region = region;
        jump_to_region();
        read_next_record();
    }
};

} // namespace bio::io::var
