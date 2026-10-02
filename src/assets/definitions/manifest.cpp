#include <assets/definitions/manifest.h>
#include <unordered_set>

namespace CE::Assets {
    std::vector<std::filesystem::path> AssetManifest::textures() const {
        // Preserve first reference order while deduplicating shared images
        // across sprite and tileset entries in this document.
        std::vector<std::filesystem::path> result;
        std::unordered_set<std::filesystem::path> seen;
        const auto append = [&result, &seen](const std::filesystem::path& path) {
            if (seen.emplace(path).second) {
                result.push_back(path);
            }
        };
        for (const auto& sprite : sprites) {
            append(sprite.texture);
        }
        for (const auto& tileset : tilesets) {
            append(tileset.texture);
        }
        return result;
    }
}
