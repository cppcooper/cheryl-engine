#include <core/resources/asset-management/manifest-loader.h>
#include <core/resources/asset-management/file-registry.h>
#include "manifest-parser.h"
#include <internals/exceptions.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_set>

namespace CE::Assets {
    namespace {
        namespace fs = std::filesystem;
        using json = nlohmann::json;

        [[noreturn]] void fail(const fs::path& source, const std::string_view location, const std::string_view message) {
            throw Exceptions::runtime_exception(
                CE_HERE, "Graphics manifest '" + source.string() + "' at " + std::string(location) + ": " + std::string(message)
            );
        }

        const json& required(const json& object, const std::string_view key, const fs::path& source) {
            const auto found = object.find(key);
            if (found == object.end())
                fail(source, "$", "missing required property '" + std::string(key) + "'");
            return *found;
        }

        json read_index(const fs::path& source) {
            std::ifstream input(source);
            if (!input.is_open())
                fail(source, "$", "unable to open index");
            try {
                auto index = json::parse(input);
                if (!index.is_object())
                    fail(source, "$", "expected an object");
                for (const auto& item : index.items()) {
                    const auto& key = item.key();
                    if (key != "$schema" && key != "version" && key != "manifests")
                        fail(source, "$", "unknown property '" + key + "'");
                }
                if (index.contains("$schema") && (!index.at("$schema").is_string() || index.at("$schema").get<std::string>().empty()))
                    fail(source, "$.$schema", "expected a nonempty schema identifier");
                const auto& version = required(index, "version", source);
                if (!version.is_string() || version.get<std::string>() != "2.0")
                    fail(source, "$.version", "expected graphics index version '2.0'");
                static_cast<void>(ManifestDetail::graphics_directory(source));
                if (!required(index, "manifests", source).is_array())
                    fail(source, "$.manifests", "expected an array of relative JSON paths");
                return index;
            } catch (const json::exception& error) {
                fail(source, "$", "unable to parse JSON: " + std::string(error.what()));
            }
        }

        fs::path registered_document(const json& value, const fs::path& source, const std::string& location, const FileRegistry& files) {
            if (!value.is_string() || value.get<std::string>().empty())
                fail(source, location, "expected a nonempty relative JSON path");
            const auto text = value.get<std::string>();
            const auto path = ManifestDetail::graphics_path(source, text, location);
            auto extension = path.extension().string();
            std::ranges::transform(extension, extension.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (extension != ".json")
                fail(source, location, "referenced graphics documents must be JSON files");
            const auto document = files.get_file_at(path);
            if (!document)
                fail(source, location, "referenced document is not registered: '" + text + "'");
            if (document->filename() == "graphics-manifests.json")
                fail(source, location, "reference graphics definitions, not another graphics index");
            return *document;
        }
    }

    fs::path ManifestDetail::graphics_directory(const fs::path& source) {
        auto directory = source.parent_path().lexically_normal();
        while (!directory.empty()) {
            if (directory.filename() == "graphics")
                return directory;
            const auto parent = directory.parent_path();
            if (parent == directory)
                break;
            directory = parent;
        }
        fail(source, "$", "graphics documents must be located under a graphics directory");
    }

    fs::path ManifestDetail::graphics_path(const fs::path& source, const std::string_view reference, const std::string_view location) {
        if (reference.empty() || reference.contains('\\') || reference.contains(':'))
            fail(source, location, "expected a nonempty graphics-relative path with forward slashes");
        const fs::path relative(reference);
        if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory())
            fail(source, location, "path must be relative to the enclosing graphics directory");
        for (const auto& part : relative) {
            if (part == "..")
                fail(source, location, "path cannot traverse to a parent directory");
        }
        return (graphics_directory(source) / relative).lexically_normal();
    }

    std::vector<AssetManifest> ManifestLoader::load_graphics(const FileRegistry& files) {
        return load_graphics_definitions(files).assets;
    }

    GraphicsDefinitions ManifestLoader::load_graphics_definitions(const FileRegistry& files, const std::vector<fs::path>& indexes) {
        GraphicsDefinitions result;
        std::unordered_set<std::filesystem::path> loaded;
        const auto selected = indexes.empty() ? files.get_files_named("graphics-manifests.json") : indexes;
        for (const auto& requested : selected) {
            const auto registered = files.get_file_at(requested);
            if (!registered || registered->filename() != "graphics-manifests.json")
                fail(requested, "$", "selection must name a registered graphics-manifests.json index");
            const auto& source = *registered;
            const auto index = read_index(source);
            const auto& documents = index.at("manifests");
            for (std::size_t i = 0; i < documents.size(); ++i) {
                const auto document = registered_document(documents[i], source, "$.manifests[" + std::to_string(i) + "]", files);
                if (!loaded.emplace(document).second)
                    continue;
                std::ifstream input(document);
                if (!input)
                    fail(document, "$", "unable to open selected definition");
                const auto root = ManifestDetail::read_document(input, document);
                if (!root.is_object())
                    fail(document, "$", "expected a definition object");
                const auto& asset_class = required(root, "asset_class", document);
                if (!asset_class.is_string())
                    fail(document, "$.asset_class", "expected a graphics asset class string");
                if (asset_class == "sprite-tileset")
                    result.assets.push_back(ManifestDetail::parse_asset(root, document));
                else if (asset_class == "shader")
                    result.shaders.push_back(ManifestDetail::parse_shader(root, document));
                else
                    fail(document, "$.asset_class", "unsupported graphics asset class");
            }
        }
        return result;
    }
}
