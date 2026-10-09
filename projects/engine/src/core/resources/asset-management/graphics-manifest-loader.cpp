#include <core/resources/asset-management/manifest-loader.h>
#include <core/resources/asset-management/file-registry.h>
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
                CE_HERE, "Graphics manifest index '" + source.string() + "' at " + std::string(location) + ": " + std::string(message)
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
                if (!version.is_string() || version.get<std::string>() != "1.0")
                    fail(source, "$.version", "expected graphics index version '1.0'");
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
            if (text.find('\\') != std::string::npos || text.find(':') != std::string::npos)
                fail(source, location, "use a relative path with forward slashes");
            const fs::path relative(text);
            if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory())
                fail(source, location, "absolute document paths are not allowed");
            auto extension = relative.extension().string();
            std::ranges::transform(extension, extension.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (extension != ".json")
                fail(source, location, "referenced graphics documents must be JSON files");
            const auto document = files.get_file_at(source.parent_path() / relative);
            if (!document)
                fail(source, location, "referenced document is not registered: '" + text + "'");
            if (document->filename() == "graphics-manifests.json")
                fail(source, location, "reference graphics definitions, not another graphics index");
            return *document;
        }
    }

    std::vector<AssetManifest> ManifestLoader::load_graphics(const FileRegistry& files) {
        std::vector<AssetManifest> result;
        std::unordered_set<std::filesystem::path> loaded;
        for (const auto& source : files.get_files_named("graphics-manifests.json")) {
            const auto index = read_index(source);
            const auto& documents = index.at("manifests");
            for (std::size_t i = 0; i < documents.size(); ++i) {
                const auto document = registered_document(documents[i], source, "$.manifests[" + std::to_string(i) + "]", files);
                if (loaded.emplace(document).second)
                    result.push_back(load(document));
            }
        }
        return result;
    }
}
