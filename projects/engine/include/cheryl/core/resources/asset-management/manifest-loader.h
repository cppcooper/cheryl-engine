#pragma once

#include <assets/definitions/manifest.h>
#include <assets/definitions/shader-assets.h>

#include <filesystem>
#include <istream>
#include <vector>

namespace CE::Assets {
    class FileRegistry;
    struct GraphicsDefinitions {
        std::vector<AssetManifest> assets;
        std::vector<ShaderAssetManifest> shaders;
    };
    /** Parse one schema 1.0 document into backend-independent definitions. Document-local
     * references are checked here; duplicate IDs across documents and image bounds are checked
     * by Loader after it has collected the full asset set.
     */
    struct ManifestLoader {
        [[nodiscard]] static AssetManifest load(const std::filesystem::path& file);
        [[nodiscard]] static AssetManifest parse(std::istream& input, const std::filesystem::path& source);
        [[nodiscard]] static ShaderAssetManifest load_shader(const std::filesystem::path& file);
        [[nodiscard]] static ShaderAssetManifest parse_shader(std::istream& input, const std::filesystem::path& source);

        // Open only registered graphics-manifests.json indexes and the graphics
        // documents they list. References are index-relative exact registered
        // paths; repeated references are parsed once across the entire batch.
        [[nodiscard]] static std::vector<AssetManifest> load_graphics(const FileRegistry& files);
        // Empty selection uses every registered index. Explicit indexes permit a
        // focused batch while references still resolve against the full registry.
        [[nodiscard]] static GraphicsDefinitions load_graphics_definitions(
            const FileRegistry& files, const std::vector<std::filesystem::path>& indexes = {}
        );
    };
}
