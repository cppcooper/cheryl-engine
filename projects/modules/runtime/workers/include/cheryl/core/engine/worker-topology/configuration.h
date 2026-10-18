#pragma once

#include <cheryl/export.h>

#include <core/engine/worker-configuration.h>

#include <filesystem>
#include <istream>
#include <string_view>

namespace CE::Workers::Json {
    // Version 1.0 settings; no SDK types or discovery, threads, group creation or
    // affinity changes. Runtime availability is resolved by CE::Workers::Pool/make_group.
    [[nodiscard]] CE_WORKER_TOPOLOGY_JSON_API CE::Workers::Configuration
    parse_configuration(std::string_view document, const std::filesystem::path& source = {});
    [[nodiscard]] CE_WORKER_TOPOLOGY_JSON_API CE::Workers::Configuration
    load_configuration(std::istream& input, const std::filesystem::path& source = {});
    [[nodiscard]] CE_WORKER_TOPOLOGY_JSON_API CE::Workers::Configuration load_configuration(const std::filesystem::path& source);
}

/**
 * @fn CE::Workers::Configuration CE::Workers::Json::parse_configuration(std::string_view document, const std::filesystem::path& source)
 * @brief Parse one strict versioned worker configuration from owned/copied text.
 * @param document Borrowed JSON bytes consumed during this call.
 * @param source Optional diagnostic label, not a file to open.
 * @return Owned inert version1.0 settings; creates no threads/groups, discovers no topology and applies no CPU policy.
 * @throws CE::Exceptions::invalid_args For malformed JSON, duplicate/unknown properties, unsupported version, invalid names/options or read/open errors.
 * Allocation failures can propagate. Runtime availability and required placement are resolved separately when constructing a pool/group. Link Cheryl::Workers with JSON enabled; no upstream JSON types enter the API.
 * @see [Detailed guide](cheryl-guide:modules/workers)
 */

/**
 * @fn CE::Workers::Configuration CE::Workers::Json::load_configuration(std::istream& input, const std::filesystem::path& source)
 * @brief Consume a JSON configuration stream without owning the stream.
 * @param input Readable stream; read failures reject rather than publish partial settings.
 * @param source Optional diagnostic filename/label, not separately opened.
 * @return Owned inert version1.0 settings; creates no threads/groups, discovers no topology and applies no CPU policy.
 * @throws CE::Exceptions::invalid_args For malformed JSON, duplicate/unknown properties, unsupported version, invalid names/options or read/open errors.
 * Allocation failures can propagate. Runtime availability and required placement are resolved separately when constructing a pool/group. Link Cheryl::Workers with JSON enabled; no upstream JSON types enter the API.
 * @see [Detailed guide](cheryl-guide:modules/workers)
 */

/**
 * @fn CE::Workers::Configuration CE::Workers::Json::load_configuration(const std::filesystem::path& source)
 * @brief Open and parse a strict JSON configuration file synchronously.
 * @param source File to open; relative paths use the caller's working directory.
 * Keep the file stable while reading. This can block and belongs at configuration/loading boundaries.
 * @return Owned inert version1.0 settings; creates no threads/groups, discovers no topology and applies no CPU policy.
 * @throws CE::Exceptions::invalid_args For malformed JSON, duplicate/unknown properties, unsupported version, invalid names/options or read/open errors.
 * Allocation failures can propagate. Runtime availability and required placement are resolved separately when constructing a pool/group. Link Cheryl::Workers with JSON enabled; no upstream JSON types enter the API.
 * @see [Detailed guide](cheryl-guide:modules/workers)
 */
