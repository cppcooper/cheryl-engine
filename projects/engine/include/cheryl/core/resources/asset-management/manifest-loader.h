#pragma once

#include <assets/definitions/manifest.h>
#include <assets/definitions/shader-assets.h>

#include <filesystem>
#include <istream>
#include <vector>

namespace CE::Assets::Loading {
    class FileRegistry;
}

namespace CE::Assets::Manifests {
    struct Definitions {
        std::vector<Graphics> assets;
        std::vector<CE::Assets::Shaders::Manifest> shaders;
    };
    /** Parse version 2.0 sprite/tileset manifests into backend-independent definitions.
     * Texture paths resolve from the enclosing graphics directory. Duplicate IDs across
     * documents and image bounds are checked by Loader after it has collected the full asset set.
     */
    struct Reader {
        [[nodiscard]] static Graphics load(const std::filesystem::path& file);
        [[nodiscard]] static Graphics parse(std::istream& input, const std::filesystem::path& source);
        [[nodiscard]] static CE::Assets::Shaders::Manifest load_shader(const std::filesystem::path& file);
        [[nodiscard]] static CE::Assets::Shaders::Manifest parse_shader(std::istream& input, const std::filesystem::path& source);

        // Open only registered graphics-manifests.json indexes and the graphics
        // documents they list. References are graphics-relative exact registered
        // paths; repeated references are parsed once across the entire batch.
        [[nodiscard]] static std::vector<Graphics> load_graphics(const CE::Assets::Loading::FileRegistry& files);
        // Empty selection uses every registered index. Explicit indexes permit a
        // focused batch while references still resolve against the full registry.
        [[nodiscard]] static Definitions
        load_graphics_definitions(const CE::Assets::Loading::FileRegistry& files, const std::vector<std::filesystem::path>& indexes = {});
    };
}
