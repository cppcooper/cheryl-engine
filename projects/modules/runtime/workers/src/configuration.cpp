#include <core/engine/worker-topology/configuration.h>
#include <internals/exceptions.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace CE::Workers::Json {
    namespace {
        using Json = nlohmann::json;
        namespace fs = std::filesystem;

        [[noreturn]] void fail(const std::filesystem::path& source, const std::string& field, const std::string& reason) {
            const auto name = source.empty() ? "<worker configuration>" : source.string();
            throw Exceptions::invalid_args(CE_HERE, "Worker configuration '" + name + "' at " + field + ": " + reason);
        }

        void fields(
            const Json& value,
            const std::filesystem::path& source,
            const std::string& field,
            const std::initializer_list<std::string_view> allowed
        ) {
            if (!value.is_object())
                fail(source, field, "expected an object");
            for (const auto& item : value.items())
                if (std::ranges::find(allowed, item.key()) == allowed.end())
                    fail(source, field + '.' + item.key(), "unknown property");
        }

        std::string text(const Json& value, const std::filesystem::path& source, const std::string& field) {
            if (!value.is_string() || value.get_ref<const std::string&>().empty())
                fail(source, field, "expected a nonempty string");
            return value.get<std::string>();
        }

        template <typename T> T integer(const Json& value, const std::filesystem::path& source, const std::string& field) {
            if (!value.is_number_integer() || (!value.is_number_unsigned() && value.get<std::int64_t>() < 0))
                fail(source, field, "expected a nonnegative integer");
            const auto number = value.get<std::uint64_t>();
            if (number > std::numeric_limits<T>::max())
                fail(source, field, "integer is outside the supported range");
            return static_cast<T>(number);
        }

        std::size_t concurrency(const Json& value, const std::filesystem::path& source, const std::string& field) {
            if (value.is_string()) {
                if (text(value, source, field) == "auto")
                    return 0;
                fail(source, field, "expected 'auto' or a positive integer");
            }
            const auto number = integer<std::size_t>(value, source, field);
            if (number == 0)
                fail(source, field, "expected a positive integer; use 'auto' for automatic capacity");
            return number;
        }

        template <typename T>
        T choice(
            const Json& value,
            const std::filesystem::path& source,
            const std::string& field,
            const std::initializer_list<std::pair<std::string_view, T>> choices
        ) {
            const auto name = text(value, source, field);
            for (const auto& [key, result] : choices)
                if (name == key)
                    return result;
            fail(source, field, "unsupported value '" + name + "'");
        }

        bool boolean(const Json& value, const std::filesystem::path& source, const std::string& field) {
            if (!value.is_boolean())
                fail(source, field, "expected a boolean");
            return value.get<bool>();
        }

        CE::Workers::GroupOptions group_options(const Json& value, const std::filesystem::path& source, const std::string& field) {
            fields(value, source, field, {"concurrency", "weight", "priority", "locality", "cpu"});
            CE::Workers::GroupOptions options;
            if (value.contains("concurrency"))
                options.max_concurrency = concurrency(value.at("concurrency"), source, field + ".concurrency");
            if (value.contains("weight")) {
                options.weight = integer<unsigned int>(value.at("weight"), source, field + ".weight");
                if (options.weight == 0 || options.weight > 1024)
                    fail(source, field + ".weight", "expected 1..1024");
            }
            if (value.contains("priority")) {
                options.priority = integer<unsigned int>(value.at("priority"), source, field + ".priority");
                if (options.priority > 7)
                    fail(source, field + ".priority", "expected 0..7");
            }
            if (value.contains("locality"))
                options.cpu.locality = choice<CE::Workers::LocalityPolicy>(
                    value.at("locality"), source, field + ".locality",
                    {{"automatic", CE::Workers::LocalityPolicy::Automatic},
                     {"reuse", CE::Workers::LocalityPolicy::Reuse},
                     {"spread", CE::Workers::LocalityPolicy::Spread}}
                );
            if (value.contains("cpu")) {
                const auto& cpu = value.at("cpu");
                fields(cpu, source, field + ".cpu", {"cpus", "strength", "cache_domain", "prefer_same_worker"});
                if (cpu.contains("cpus")) {
                    const auto& cpus = cpu.at("cpus");
                    if (!cpus.is_array())
                        fail(source, field + ".cpu.cpus", "expected an array of OS CPU IDs");
                    for (std::size_t index = 0; index < cpus.size(); ++index)
                        options.cpu.cpus.push_back(
                            integer<unsigned int>(cpus[index], source, field + ".cpu.cpus[" + std::to_string(index) + ']')
                        );
                }
                if (cpu.contains("strength"))
                    options.cpu.strength = choice<CE::Workers::PolicyStrength>(
                        cpu.at("strength"), source, field + ".cpu.strength",
                        {{"preferred", CE::Workers::PolicyStrength::Preferred}, {"required", CE::Workers::PolicyStrength::Required}}
                    );
                if (cpu.contains("cache_domain"))
                    options.cpu.shared_cache_domain = integer<unsigned int>(cpu.at("cache_domain"), source, field + ".cpu.cache_domain");
                if (cpu.contains("prefer_same_worker"))
                    options.cpu.prefer_same_worker = boolean(cpu.at("prefer_same_worker"), source, field + ".cpu.prefer_same_worker");
            }
            try {
                CE::Workers::validate_group_options(options);
            } catch (const Exceptions::invalid_args& error) {
                fail(source, field, error.what());
            }
            return options;
        }

        Json read_document(std::istream& input, const std::filesystem::path& source) {
            std::vector<std::unordered_set<std::string>> keys;
            try {
                auto root = Json::parse(input, [&](const int, const Json::parse_event_t event, Json& parsed) {
                    if (event == Json::parse_event_t::object_start)
                        keys.emplace_back();
                    else if (event == Json::parse_event_t::object_end)
                        keys.pop_back();
                    else if (event == Json::parse_event_t::key && !keys.back().insert(parsed.get<std::string>()).second)
                        fail(source, "$", "duplicate JSON property '" + parsed.get<std::string>() + "'");
                    return true;
                });
                if (input.bad())
                    fail(source, "$", "reading the configuration stream failed");
                return root;
            } catch (const Json::exception& error) {
                fail(source, "$", "unable to parse JSON: " + std::string(error.what()));
            } catch (const std::ios_base::failure& error) {
                fail(source, "$", "reading the configuration stream failed: " + std::string(error.what()));
            }
        }
    }

    CE::Workers::Configuration load_configuration(std::istream& input, const std::filesystem::path& source) {
        const auto root = read_document(input, source);
        fields(root, source, "$", {"$schema", "version", "workers", "groups"});
        if (!root.contains("version"))
            fail(source, "$.version", "missing schema version");
        if (text(root.at("version"), source, "$.version") != "1.0")
            fail(source, "$.version", "unsupported version; expected '1.0'");
        if (root.contains("$schema"))
            static_cast<void>(text(root.at("$schema"), source, "$.$schema"));
        CE::Workers::Configuration result;
        if (root.contains("workers"))
            result.pool.worker_count = concurrency(root.at("workers"), source, "$.workers");
        if (root.contains("groups")) {
            const auto& groups = root.at("groups");
            if (!groups.is_object())
                fail(source, "$.groups", "expected an object of named worker groups");
            for (const auto& item : groups.items()) {
                const auto field = "$.groups." + item.key();
                if (!CE::Workers::is_group_name(item.key()))
                    fail(source, field, "group name must match [a-z][a-z0-9_]*");
                result.groups.emplace(item.key(), group_options(item.value(), source, field));
            }
        }
        CE::Workers::validate_configuration(result);
        return result;
    }

    CE::Workers::Configuration load_configuration(const std::filesystem::path& source) {
        std::ifstream input(source);
        if (!input)
            fail(source, "$", "unable to open configuration file");
        return load_configuration(input, source);
    }

    CE::Workers::Configuration parse_configuration(const std::string_view document, const std::filesystem::path& source) {
        std::istringstream input{std::string(document)};
        return load_configuration(input, source);
    }
}
