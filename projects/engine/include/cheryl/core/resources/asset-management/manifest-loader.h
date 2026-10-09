#pragma once

#include <assets/definitions/manifest.h>

#include <filesystem>
#include <istream>
#include <vector>

namespace CE::Assets {
    class FileRegistry;
    /** Parse one schema 1.0 document into backend-independent definitions. Document-local
     * references are checked here; duplicate IDs across documents and image bounds are checked
     * by Loader after it has collected the full asset set.
     */
    struct ManifestLoader {
        [[nodiscard]] static AssetManifest load(const std::filesystem::path& file);
        [[nodiscard]] static AssetManifest parse(std::istream& input, const std::filesystem::path& source);

        // Open only registered graphics-manifests.json indexes and the graphics
        // documents they list. References are index-relative exact registered
        // paths; repeated references are parsed once across the entire batch.
        [[nodiscard]] static std::vector<AssetManifest> load_graphics(const FileRegistry& files);
    };
}
