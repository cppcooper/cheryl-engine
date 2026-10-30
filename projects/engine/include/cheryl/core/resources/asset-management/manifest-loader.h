#pragma once

#include <cheryl/export.h>

#include <assets/definitions/manifest.h>
#include <assets/definitions/shader-assets.h>

#include <filesystem>
#include <istream>
#include <vector>

namespace CE::Assets::Loading {
    class FileRegistry;
}

namespace CE::Assets::Manifests {
    /** Owned selected graphics and shader definitions; no decoded images or native resources. */
    struct Definitions {
        std::vector<Graphics> assets; ///< Graphics documents in selected index/reference order.
        std::vector<CE::Assets::Shaders::Manifest> shaders; ///< Shader documents selected by those indexes.
    };
    /** Parse version 2.0 sprite/tileset manifests into backend-independent definitions.
     * Texture paths resolve from the enclosing graphics directory. Duplicate IDs across
     * documents and image bounds are checked by Loader after it has collected the full asset set.
     */
    struct Reader {
        [[nodiscard]] CE_API static Graphics load(const std::filesystem::path& file);
        [[nodiscard]] CE_API static Graphics parse(std::istream& input, const std::filesystem::path& source);
        [[nodiscard]] CE_API static CE::Assets::Shaders::Manifest load_shader(const std::filesystem::path& file);
        [[nodiscard]] CE_API static CE::Assets::Shaders::Manifest parse_shader(std::istream& input, const std::filesystem::path& source);

        // Open only registered graphics-manifests.json indexes and the graphics
        // documents they list. References are graphics-relative exact registered
        // paths; repeated references are parsed once across the entire batch.
        [[nodiscard]] CE_API static std::vector<Graphics> load_graphics(const CE::Assets::Loading::FileRegistry& files);
        // Empty selection uses every registered index. Explicit indexes permit a
        // focused batch while references still resolve against the full registry.
        [[nodiscard]] CE_API static Definitions
        load_graphics_definitions(const CE::Assets::Loading::FileRegistry& files, const std::vector<std::filesystem::path>& indexes = {});
    };
}

/**
 * @fn CE::Assets::Manifests::Graphics CE::Assets::Manifests::Reader::load(const std::filesystem::path& file)
 * @brief Open and parse one graphics schema/version 2.0 document on the CPU.
 * @param file Manifest origin used to resolve graphics-root-relative texture references.
 * @return Owned definitions; no discovery/cache/native state changes occur.
 * Unreadable/malformed/unsupported data throws runtime_exception; full-batch duplicate identities and decoded image bounds remain Loader validation.
 * @see [Graphics manifests](cheryl-guide:assets/asset-manifests)
 */

/**
 * @fn CE::Assets::Manifests::Graphics CE::Assets::Manifests::Reader::parse(std::istream& input, const std::filesystem::path& source)
 * @brief Parse a graphics document from borrowed stream storage.
 * @param input Stream consumed during parsing; bytes are not retained.
 * @param source Document origin for diagnostics and graphics-root-relative references.
 * @return Owned normalized definitions; parsing alone performs no resource upload or batch/cache validation.
 * Malformed/schema/field errors throw runtime_exception; allocation/stream-related failures propagate.
 */

/**
 * @fn CE::Assets::Shaders::Manifest CE::Assets::Manifests::Reader::load_shader(const std::filesystem::path& file)
 * @brief Open and parse one shader/material recipe document without native construction.
 * @param file Document origin for diagnostics/source-path resolution.
 * @return Owned recipes; Loader later snapshots selected source-file bytes.
 * Unreadable/malformed/unsupported data rejects parsing; no shader is compiled or catalogue entry published.
 * @see [Shader definitions](cheryl-guide:assets/asset-manifests#shader-material-definitions)
 */

/**
 * @fn CE::Assets::Shaders::Manifest CE::Assets::Manifests::Reader::parse_shader(std::istream& input, const std::filesystem::path& source)
 * @brief Parse shader/material definitions from a borrowed stream.
 * @param input JSON stream consumed during this call.
 * @param source Origin for diagnostics and source-root-relative path resolution.
 * @return Owned recipes, without native program/material resources.
 * Syntax/schema/field failures reject; backend capability/reflection checks remain the provider builder's responsibility.
 */

/**
 * @fn std::vector<CE::Assets::Manifests::Graphics> CE::Assets::Manifests::Reader::load_graphics(const CE::Assets::Loading::FileRegistry& files)
 * @brief Load all registered graphics indexes and return their selected graphics documents.
 * @param files Stable registered exact paths; unrelated JSON is not treated as a manifest.
 * @return Owned graphics definitions, deduplicated by referenced document across the batch.
 * References are graphics-relative exact registered paths; unknown/malformed/index/schema data rejects. Shader definitions are not returned by this compatibility projection.
 */

/**
 * @fn CE::Assets::Manifests::Definitions CE::Assets::Manifests::Reader::load_graphics_definitions(const CE::Assets::Loading::FileRegistry& files, const std::vector<std::filesystem::path>& indexes)
 * @brief Resolve registered graphics-manifests.json selections and parse referenced graphics/shader documents once.
 * @param files Full stable discovery registry, including selected indexes and their exact references.
 * @param indexes Empty selects every registered index; explicit paths select a focused batch.
 * @return Owned selected definitions without image decoding, shader compilation or runtime publication.
 * Unknown/nonindex selections, missing/unregistered references or malformed documents throw; allocation/I/O failures propagate.
 * @see [Index selection](cheryl-guide:assets/asset-manifests#index-selection-and-paths)
 */
