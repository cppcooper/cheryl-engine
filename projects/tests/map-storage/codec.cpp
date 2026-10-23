#include "codec.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <set>
#include <string_view>
#include <utility>

static_assert(NLOHMANN_JSON_VERSION_MAJOR == 3 && NLOHMANN_JSON_VERSION_MINOR == 11 && NLOHMANN_JSON_VERSION_PATCH == 3);

namespace MapFixture {
    namespace {
        std::string diagnostic_path(const std::filesystem::path& origin) {
            auto text = origin.string();
            for (std::size_t at = 0; (at = text.find('\0', at)) != std::string::npos; at += 2)
                text.replace(at, 1, "\\0");
            return text;
        }
    }

    Error::Error(Code code, std::string operation, std::filesystem::path origin, std::string location,
                 const std::string& detail, std::optional<std::size_t> byte_offset, std::error_code io_error)
    : std::runtime_error(operation + " '" + diagnostic_path(origin) + "' at " + location + ": " + detail),
      code(code), operation(std::move(operation)), origin(std::move(origin)), location(std::move(location)),
      byte_offset(byte_offset), io_error(io_error) {}

    namespace {
        using Json = nlohmann::json;

        struct Context {
            std::string operation;
            std::filesystem::path origin;

            [[noreturn]] void fail(Code code, const std::string& location, const std::string& detail,
                                  std::optional<std::size_t> offset = {}, std::error_code io = {}) const {
                throw Error(code, operation, origin, location, detail, offset, io);
            }
        };

        std::string child(const std::string& path, const std::string& key) {
            const bool plain = !key.empty() && std::ranges::all_of(key, [](unsigned char ch) {
                return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
            });
            return plain ? path + '.' + key : path + '[' + Json(key).dump() + ']';
        }

        std::string element(const std::string& path, std::size_t index) { return path + '[' + std::to_string(index) + ']'; }

        std::string acquire(std::istream& input, const Context& context) {
            if (!input.rdbuf() || input.bad() || (input.fail() && !input.eof()))
                context.fail(Code::Read, "$", "input stream is not readable", 1);
            std::string bytes;
            for (std::size_t count = 0; count <= max_document_bytes; ++count) {
                std::char_traits<char>::int_type value;
                try {
                    // Bypass istream's catch-all badbit conversion: a streambuf's
                    // bad_alloc/logic_error must not become a rejected document.
                    value = input.rdbuf()->sbumpc();
                } catch (const std::ios_base::failure& error) {
                    context.fail(Code::Read, "$", error.what(), count + 1, error.code());
                }
                if (std::char_traits<char>::eq_int_type(value, std::char_traits<char>::eof()))
                    return bytes;
                if (count == max_document_bytes)
                    context.fail(Code::LimitExceeded, "$", "document exceeds 8 MiB", count + 1);
                const char ch = std::char_traits<char>::to_char_type(value);
                if (ch == '\0')
                    context.fail(Code::Malformed, "$", "raw NUL is forbidden", count + 1);
                bytes.push_back(ch);
            }
            throw std::logic_error("unreachable bounded input state");
        }

        // Own each unfinished container separately. Charge budgets before appending
        // DOM entries; throw to reject rather than using a discarding DOM callback.
        class BoundedDocument final : public nlohmann::json_sax<Json> {
            struct Frame {
                Json value;
                std::string path;
                std::string key;
                std::set<std::string> names;
            };
            const Context& context_;
            std::vector<Frame> frames_;
            std::size_t nodes_ = 0;
            Json root_;

            std::string next() const {
                if (frames_.empty())
                    return "$";
                const auto& frame = frames_.back();
                return frame.value.is_array() ? element(frame.path, frame.value.size()) : child(frame.path, frame.key);
            }
            void node(const std::string& path) {
                if (nodes_ == max_nodes)
                    context_.fail(Code::LimitExceeded, path, "JSON node budget exceeded");
                ++nodes_;
            }
            void value_slot(const std::string& path) {
                if (!frames_.empty() && frames_.back().value.is_array() && frames_.back().value.size() == max_array_elements)
                    context_.fail(Code::LimitExceeded, path, "JSON array element budget exceeded");
                node(path);
            }
            void append(Json value) {
                if (frames_.empty())
                    root_ = std::move(value);
                else if (frames_.back().value.is_array())
                    frames_.back().value.push_back(std::move(value));
                else
                    frames_.back().value[frames_.back().key] = std::move(value);
            }
            bool scalar(Json value) {
                value_slot(next());
                append(std::move(value));
                return true;
            }
            bool start(bool object) {
                const auto path = next();
                if (frames_.size() == max_depth)
                    context_.fail(Code::LimitExceeded, path, "JSON nesting depth exceeds 16");
                value_slot(path);
                frames_.push_back({object ? Json::object() : Json::array(), path, {}, {}});
                return true;
            }
            bool finish() {
                auto value = std::move(frames_.back().value);
                frames_.pop_back();
                append(std::move(value));
                return true;
            }
            void string_size(const std::string& value, const std::string& path) const {
                if (value.size() > max_key_bytes)
                    context_.fail(Code::LimitExceeded, path, "decoded string exceeds 128 bytes");
            }

        public:
            explicit BoundedDocument(const Context& context) : context_(context) {}
            Json take() { return std::move(root_); }
            bool null() override { return scalar(nullptr); }
            bool boolean(bool value) override { return scalar(value); }
            bool number_integer(number_integer_t value) override { return scalar(value); }
            bool number_unsigned(number_unsigned_t value) override { return scalar(value); }
            bool number_float(number_float_t value, const string_t&) override { return scalar(value); }
            bool string(string_t& value) override {
                string_size(value, next());
                return scalar(std::move(value));
            }
            bool binary(binary_t&) override { throw std::logic_error("binary event in textual JSON"); }
            bool start_object(std::size_t) override { return start(true); }
            bool start_array(std::size_t) override { return start(false); }
            bool end_object() override { return finish(); }
            bool end_array() override { return finish(); }
            bool key(string_t& value) override {
                auto& frame = frames_.back();
                // Limit before constructing a potentially long diagnostic path.
                string_size(value, frame.path);
                const auto path = child(frame.path, value);
                if (frame.names.contains(value))
                    context_.fail(Code::InvalidValue, path, "duplicate decoded object member");
                if (frame.names.size() == max_object_members)
                    context_.fail(Code::LimitExceeded, path, "JSON object member budget exceeded");
                node(path);
                frame.names.insert(value);
                frame.key = std::move(value);
                return true;
            }
            bool parse_error(std::size_t position, const std::string&, const Json::exception& error) override {
                context_.fail(error.id == 406 ? Code::InvalidValue : Code::Malformed, next(), error.what(), position);
            }
        };

        void properties(const Json& value, const std::string& path, const Context& context,
                        std::initializer_list<std::string_view> allowed) {
            if (!value.is_object())
                context.fail(Code::InvalidValue, path, "expected an object");
            for (const auto& item : value.items())
                if (std::ranges::find(allowed, item.key()) == allowed.end())
                    context.fail(Code::InvalidValue, child(path, item.key()), "unknown member");
        }

        const Json& required(const Json& value, std::string_view key, const std::string& path, const Context& context) {
            const auto found = value.find(key);
            if (found == value.end())
                context.fail(Code::InvalidValue, child(path, std::string(key)), "missing required member");
            return *found;
        }

        const std::string& text(const Json& value, const std::string& path, const Context& context) {
            if (!value.is_string())
                context.fail(Code::InvalidValue, path, "expected a string");
            return value.get_ref<const std::string&>();
        }

        std::uint32_t integer(const Json& value, const std::string& path, const Context& context) {
            if (!value.is_number_integer() || (!value.is_number_unsigned() && value.get<std::int64_t>() < 0))
                context.fail(Code::InvalidValue, path, "expected a nonnegative integer encoding");
            const auto number = value.get<std::uint64_t>();
            if (number > std::numeric_limits<std::uint32_t>::max())
                context.fail(Code::InvalidValue, path, "integer exceeds uint32 range");
            return static_cast<std::uint32_t>(number);
        }

        void key(const std::string& value, bool qualified, const std::string& path, const Context& context) {
            if (value.size() > max_key_bytes)
                context.fail(Code::LimitExceeded, path, "key exceeds 128 bytes");
            const auto part = [](std::string_view name, bool dot) {
                const auto alnum = [](char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9'); };
                return !name.empty() && alnum(name.front()) && std::ranges::all_of(name, [&](char ch) {
                    return alnum(ch) || ch == '_' || ch == '-' || (dot && ch == '.');
                });
            };
            bool valid = part(value, false);
            if (qualified) {
                const auto colon = value.find(':');
                valid = colon != std::string::npos && part(std::string_view(value).substr(0, colon), true) &&
                        part(std::string_view(value).substr(colon + 1), false);
            }
            if (!valid)
                context.fail(Code::InvalidValue, path, "invalid logical key");
        }

        std::size_t dimensions(std::uint32_t width, std::uint32_t height, std::size_t layers, const Context& context) {
            if (!width || !height)
                context.fail(Code::InvalidValue, !width ? "$.width" : "$.height", "dimension must be positive");
            if (width > max_dimension || height > max_dimension)
                context.fail(Code::LimitExceeded, width > max_dimension ? "$.width" : "$.height", "dimension exceeds 1024");
            if (!layers)
                context.fail(Code::InvalidValue, "$.layers", "at least one layer is required");
            if (layers > max_layers)
                context.fail(Code::LimitExceeded, "$.layers", "too many layers");
            if (height > max_cells / width)
                context.fail(Code::LimitExceeded, "$.height", "map area exceeds cell budget");
            const auto area = std::size_t{width} * height;
            if (layers > max_cells / area)
                context.fail(Code::LimitExceeded, "$.layers", "total layer cells exceed budget");
            return area;
        }

        void array(const Json& value, const std::string& path, const Context& context) {
            if (!value.is_array())
                context.fail(Code::InvalidValue, path, "expected an array");
        }

        void validate(const Snapshot& snapshot, const Context& context) {
            const auto area = dimensions(snapshot.width, snapshot.height, snapshot.layers.size(), context);
            key(snapshot.id, true, "$.id", context);
            std::set<std::string> layers;
            for (std::size_t i = 0; i < snapshot.layers.size(); ++i) {
                const auto path = element("$.layers", i);
                const auto& layer = snapshot.layers[i];
                key(layer.id, false, path + ".id", context);
                if (!layers.insert(layer.id).second)
                    context.fail(Code::InvalidValue, path + ".id", "duplicate layer identity");
                if (layer.cells.size() != area)
                    context.fail(Code::InvalidValue, path + ".cells", "cell count differs from width * height");
                for (std::size_t cell = 0; cell < area; ++cell)
                    if (layer.cells[cell])
                        key(*layer.cells[cell], true, element(path + ".cells", cell), context);
            }
            if (snapshot.spawns.size() > max_spawns)
                context.fail(Code::LimitExceeded, "$.spawns", "too many spawns");
            std::set<std::string> spawns;
            for (std::size_t i = 0; i < snapshot.spawns.size(); ++i) {
                const auto path = element("$.spawns", i);
                const auto& spawn = snapshot.spawns[i];
                key(spawn.name, false, path + ".name", context);
                key(spawn.layer, false, path + ".layer", context);
                if (!spawns.insert(spawn.name).second)
                    context.fail(Code::InvalidValue, path + ".name", "duplicate spawn identity");
                if (!layers.contains(spawn.layer))
                    context.fail(Code::InvalidValue, path + ".layer", "unresolved layer identity");
                if (spawn.x >= snapshot.width || spawn.y >= snapshot.height)
                    context.fail(Code::InvalidValue, path + (spawn.x >= snapshot.width ? ".x" : ".y"), "spawn is outside map bounds");
            }
        }

        Snapshot reconstruct(const Json& document, const Context& context) {
            if (!document.is_object())
                context.fail(Code::InvalidValue, "$", "expected a document object");
            const auto get = [&](std::string_view name) -> const Json& { return required(document, name, "$", context); };
            if (text(get("format"), "$.format", context) != "cheryl.map-fixture")
                context.fail(Code::UnsupportedFormat, "$.format", "unsupported map format");
            if (integer(get("version"), "$.version", context) != 1)
                context.fail(Code::UnsupportedVersion, "$.version", "unsupported map version");
            properties(document, "$", context, {"format", "version", "id", "width", "height", "layers", "spawns"});
            const auto width = integer(get("width"), "$.width", context);
            const auto height = integer(get("height"), "$.height", context);
            const auto& layers = get("layers");
            array(layers, "$.layers", context);
            // Check both products before any Snapshot vector allocation.
            const auto area = dimensions(width, height, layers.size(), context);
            const auto& spawns = get("spawns");
            array(spawns, "$.spawns", context);
            if (spawns.size() > max_spawns)
                context.fail(Code::LimitExceeded, "$.spawns", "too many spawns");
            Snapshot result{.id = text(get("id"), "$.id", context), .width = width, .height = height, .layers = {}, .spawns = {}};
            result.layers.reserve(layers.size());
            for (std::size_t i = 0; i < layers.size(); ++i) {
                const auto path = element("$.layers", i);
                const auto& source = layers[i];
                properties(source, path, context, {"id", "cells"});
                const auto& cells = required(source, "cells", path, context);
                array(cells, path + ".cells", context);
                if (cells.size() != area)
                    context.fail(Code::InvalidValue, path + ".cells", "cell count differs from width * height");
                Layer layer{.id = text(required(source, "id", path, context), path + ".id", context), .cells = {}};
                layer.cells.reserve(area);
                for (std::size_t cell = 0; cell < area; ++cell) {
                    if (cells[cell].is_null())
                        layer.cells.emplace_back(std::nullopt);
                    else
                        layer.cells.emplace_back(text(cells[cell], element(path + ".cells", cell), context));
                }
                result.layers.push_back(std::move(layer));
            }
            result.spawns.reserve(spawns.size());
            for (std::size_t i = 0; i < spawns.size(); ++i) {
                const auto path = element("$.spawns", i);
                const auto& source = spawns[i];
                properties(source, path, context, {"name", "layer", "x", "y"});
                result.spawns.push_back({
                    .name = text(required(source, "name", path, context), path + ".name", context),
                    .layer = text(required(source, "layer", path, context), path + ".layer", context),
                    .x = integer(required(source, "x", path, context), path + ".x", context),
                    .y = integer(required(source, "y", path, context), path + ".y", context)
                });
            }
            validate(result, context);
            return result;
        }

        class Output {
            const Context& context_;
            std::string bytes_;

        public:
            explicit Output(const Context& context) : context_(context) {}
            void append(std::string_view text, const std::string& path) {
                if (text.size() > max_document_bytes - bytes_.size())
                    context_.fail(Code::LimitExceeded, path, "encoded document exceeds 8 MiB");
                bytes_.append(text);
            }
            void quoted(const std::string& key, const std::string& path) {
                // validate() permits only ASCII identifier characters, none needing JSON escaping.
                append("\"", path);
                append(key, path);
                append("\"", path);
            }
            std::string take() { return std::move(bytes_); }
        };
    }

    Snapshot parse(std::istream& input, const std::filesystem::path& origin) {
        const Context context{"parse", origin};
        const auto bytes = acquire(input, context);
        BoundedDocument document(context);
        if (!Json::sax_parse(bytes.begin(), bytes.end(), &document, Json::input_format_t::json, true, false))
            throw std::logic_error("SAX parsing stopped without a codec error");
        return reconstruct(document.take(), context);
    }

    std::string encode(const Snapshot& snapshot) {
        const Context context{"encode", "<snapshot>"};
        validate(snapshot, context);
        Output output(context);
        output.append("{\n  \"format\": \"cheryl.map-fixture\",\n  \"version\": 1,\n  \"id\": ", "$");
        output.quoted(snapshot.id, "$.id");
        output.append(",\n  \"width\": " + std::to_string(snapshot.width) + ",\n  \"height\": " + std::to_string(snapshot.height) +
                      ",\n  \"layers\": [\n", "$");
        for (std::size_t i = 0; i < snapshot.layers.size(); ++i) {
            const auto path = element("$.layers", i);
            const auto& layer = snapshot.layers[i];
            output.append("    {\n      \"id\": ", path);
            output.quoted(layer.id, path + ".id");
            output.append(",\n      \"cells\": [", path);
            for (std::size_t cell = 0; cell < layer.cells.size(); ++cell) {
                const auto location = element(path + ".cells", cell);
                if (cell)
                    output.append(", ", location);
                if (layer.cells[cell])
                    output.quoted(*layer.cells[cell], location);
                else
                    output.append("null", location);
            }
            output.append("]\n    }", path);
            output.append(i + 1 == snapshot.layers.size() ? "\n" : ",\n", path);
        }
        output.append("  ],\n  \"spawns\": [", "$.spawns");
        for (std::size_t i = 0; i < snapshot.spawns.size(); ++i) {
            const auto path = element("$.spawns", i);
            const auto& spawn = snapshot.spawns[i];
            output.append(i ? ",\n    {\"name\": " : "\n    {\"name\": ", path);
            output.quoted(spawn.name, path + ".name");
            output.append(", \"layer\": ", path);
            output.quoted(spawn.layer, path + ".layer");
            output.append(", \"x\": " + std::to_string(spawn.x) + ", \"y\": " + std::to_string(spawn.y) + '}', path);
        }
        output.append(snapshot.spawns.empty() ? "]\n}\n" : "\n  ]\n}\n", "$");
        return output.take();
    }
}
