#include "definition-codec.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <system_error>
#include <utility>

namespace GateConsole {
    namespace {
        enum class Container { Document, Nodes, Node, Choices, Choice };
        enum class Field : unsigned { None, Schema, Id, Revision, Entry, Nodes, Text, Kind, Choices, Target, Condition, Effect };
        constexpr std::array<std::pair<Field, std::string_view>, 11> names{
            {{Field::Schema, "definition_schema"},
             {Field::Id, "id"},
             {Field::Revision, "revision"},
             {Field::Entry, "entry"},
             {Field::Nodes, "nodes"},
             {Field::Text, "text"},
             {Field::Kind, "kind"},
             {Field::Choices, "choices"},
             {Field::Target, "target"},
             {Field::Condition, "condition"},
             {Field::Effect, "effect"}}
        };
        constexpr unsigned bit(Field field) {
            return 1U << static_cast<unsigned>(field);
        }
        constexpr unsigned required(Container container) {
            switch (container) {
                case Container::Document:
                    return bit(Field::Schema) | bit(Field::Id) | bit(Field::Revision) | bit(Field::Entry) | bit(Field::Nodes);
                case Container::Node:
                    return bit(Field::Id) | bit(Field::Text) | bit(Field::Kind) | bit(Field::Choices);
                case Container::Choice:
                    return bit(Field::Id) | bit(Field::Text) | bit(Field::Target);
                default:
                    return 0;
            }
        }
        constexpr unsigned allowed(Container container) {
            return required(container) | (container == Container::Choice ? bit(Field::Condition) | bit(Field::Effect) : 0);
        }
        std::string_view name_of(Field field) {
            for (const auto& [candidate, name] : names)
                if (candidate == field)
                    return name;
            return {};
        }
        std::string location(std::optional<std::size_t> node, std::optional<std::size_t> choice, std::string_view field = {}) {
            std::string result = "$";
            if (node)
                result += ".nodes[" + std::to_string(*node) + "]";
            if (choice)
                result += ".choices[" + std::to_string(*choice) + "]";
            if (!field.empty())
                result += "." + std::string(field);
            return result;
        }

        // Fixed authored shape, streamed straight into an owned Draft. The input
        // byte cap also bounds the lexer's token buffer before each SAX callback.
        struct DefinitionSax final : nlohmann::json_sax<nlohmann::json> {
            struct Frame {
                Container container;
                unsigned seen = 0;
                Field pending = Field::None;
                std::optional<std::size_t> node;
                std::optional<std::size_t> choice;
            };
            std::array<Frame, maximum_definition_depth> frames{};
            std::size_t depth = 0;
            std::size_t choices = 0;
            std::size_t text_bytes = 0;
            std::size_t choice_budget;
            bool started = false;
            bool complete = false;
            D::Draft draft;
            DefinitionError error;

            DefinitionSax(std::string origin, std::size_t budget)
            : choice_budget(budget), error{std::move(origin), {}, {}, {}, {}, {}} {}

            std::string current_location() const {
                if (!depth)
                    return "$";
                const auto& frame = frames[depth - 1];
                if (frame.container == Container::Nodes)
                    return location(draft.nodes.size(), {});
                if (frame.container == Container::Choices)
                    return location(frame.node, draft.nodes[*frame.node].choices.size());
                return location(frame.node, frame.choice, name_of(frame.pending));
            }
            bool reject(std::string message, std::string field = {}) {
                if (error.message.empty()) {
                    error.message = std::move(message);
                    error.field = field.empty() ? current_location() : std::move(field);
                    if (depth) {
                        const auto& frame = frames[depth - 1];
                        error.node = frame.container == Container::Nodes ? std::optional(draft.nodes.size()) : frame.node;
                        error.choice =
                            frame.container == Container::Choices ? std::optional(draft.nodes[*frame.node].choices.size()) : frame.choice;
                    }
                }
                return false;
            }
            bool push(Frame frame) {
                if (depth == frames.size())
                    return reject("Definition nesting exceeds five container levels");
                frames[depth++] = std::move(frame);
                return true;
            }
            Field pending() const { return depth ? frames[depth - 1].pending : Field::None; }
            bool consumed() {
                frames[depth - 1].pending = Field::None;
                return true;
            }
            bool start_object(std::size_t) override {
                if (!started) {
                    started = true;
                    return push({Container::Document});
                }
                if (depth == frames.size())
                    return reject("Definition nesting exceeds five container levels");
                if (depth && frames[depth - 1].container == Container::Nodes) {
                    if (draft.nodes.size() == maximum_definition_nodes)
                        return reject("Definition exceeds 256 nodes");
                    const auto index = draft.nodes.size();
                    draft.nodes.emplace_back();
                    return push({Container::Node, 0, Field::None, index});
                }
                if (depth && frames[depth - 1].container == Container::Choices) {
                    if (choices == choice_budget)
                        return reject("Definition total choice budget exceeded (" + std::to_string(choice_budget) + ")");
                    const auto node = *frames[depth - 1].node;
                    const auto index = draft.nodes[node].choices.size();
                    draft.nodes[node].choices.emplace_back();
                    ++choices;
                    return push({Container::Choice, 0, Field::None, node, index});
                }
                return reject("Expected a scalar or the authored array shape, not an object");
            }
            bool start_array(std::size_t) override {
                if (depth == frames.size())
                    return reject("Definition nesting exceeds five container levels");
                if (pending() == Field::Nodes) {
                    consumed();
                    return push({Container::Nodes});
                }
                if (pending() == Field::Choices) {
                    const auto node = frames[depth - 1].node;
                    consumed();
                    return push({Container::Choices, 0, Field::None, node});
                }
                return reject("Unexpected array; only nodes and choices are arrays");
            }
            bool key(string_t& key) override {
                if (!depth)
                    return reject("Unexpected property");
                auto& frame = frames[depth - 1];
                if (key.size() > maximum_definition_id_bytes)
                    return reject("Property name exceeds 128 bytes");
                Field field = Field::None;
                for (const auto& [candidate, name] : names)
                    if (key == name && (allowed(frame.container) & bit(candidate)))
                        field = candidate;
                if (field == Field::None)
                    return reject("Unknown property: " + key, location(frame.node, frame.choice, key));
                if (frame.seen & bit(field))
                    return reject("Duplicate property: " + key, location(frame.node, frame.choice, key));
                frame.seen |= bit(field);
                frame.pending = field;
                return true;
            }
            bool end_object() override {
                if (!depth)
                    return reject("Unexpected object end");
                const auto& frame = frames[depth - 1];
                const auto missing = required(frame.container) & ~frame.seen;
                if (missing) {
                    for (const auto& [field, name] : names)
                        if (missing & bit(field))
                            return reject("Missing required property: " + std::string(name), location(frame.node, frame.choice, name));
                }
                --depth;
                if (!depth)
                    complete = true;
                return true;
            }
            bool end_array() override {
                if (!depth || (frames[depth - 1].container != Container::Nodes && frames[depth - 1].container != Container::Choices))
                    return reject("Unexpected array end");
                --depth;
                return true;
            }
            bool string(string_t& value) override {
                const auto field = pending();
                if (field == Field::Text) {
                    if (value.size() > maximum_definition_text_bytes)
                        return reject("Text exceeds 4096 decoded bytes");
                    if (value.size() > maximum_definition_total_text_bytes - text_bytes)
                        return reject("Aggregate text exceeds 32768 decoded bytes");
                    text_bytes += value.size();
                } else if (value.size() > maximum_definition_id_bytes)
                    return reject("Identifier/key exceeds 128 decoded bytes");
                if (!depth)
                    return reject("Definition must be an object");
                const auto& frame = frames[depth - 1];
                if (frame.container == Container::Document) {
                    if (field == Field::Id)
                        draft.id.value = std::move(value);
                    else if (field == Field::Entry)
                        draft.entry.value = std::move(value);
                    else
                        return reject("Expected an integer or nodes array, not a string");
                } else if (frame.container == Container::Node || frame.container == Container::Choice) {
                    auto& node = draft.nodes[*frame.node];
                    if (frame.container == Container::Node) {
                        if (field == Field::Id)
                            node.id.value = std::move(value);
                        else if (field == Field::Text)
                            node.text = std::move(value);
                        else if (field == Field::Kind) {
                            if (value != "prompt" && value != "terminal")
                                return reject("kind must be prompt or terminal");
                            node.kind = value == "prompt" ? D::NodeKind::Prompt : D::NodeKind::Terminal;
                        } else
                            return reject("Expected choices array, not a string");
                    } else {
                        auto& choice = node.choices[*frame.choice];
                        switch (field) {
                            case Field::Id:
                                choice.id.value = std::move(value);
                                break;
                            case Field::Text:
                                choice.text = std::move(value);
                                break;
                            case Field::Target:
                                choice.target.value = std::move(value);
                                break;
                            case Field::Condition:
                                choice.condition = D::ConditionKey{std::move(value)};
                                break;
                            case Field::Effect:
                                choice.effect = D::EffectKey{std::move(value)};
                                break;
                            default:
                                return reject("Unexpected string");
                        }
                    }
                } else
                    return reject("Array entries must be objects");
                return consumed();
            }
            bool number_unsigned(number_unsigned_t value) override {
                if (pending() == Field::Schema) {
                    if (value != 1)
                        return reject("Unsupported definition_schema (expected 1)");
                } else if (pending() == Field::Revision) {
                    if (!value)
                        return reject("revision must be a positive uint64 integer");
                    draft.revision = value;
                } else
                    return reject("Expected a string or authored container, not an integer");
                return consumed();
            }
            bool number_integer(number_integer_t value) override {
                if (value < 0)
                    return reject("Negative integer is out of range");
                return number_unsigned(static_cast<number_unsigned_t>(value));
            }
            bool null() override { return reject("Null is not an authored field type"); }
            bool boolean(bool) override { return reject("Booleans are not authored field types"); }
            bool number_float(number_float_t, const string_t&) override { return reject("Expected an in-range integer, not a float"); }
            bool binary(binary_t&) override { return reject("Binary data is not supported"); }
            bool parse_error(std::size_t position, const std::string&, const nlohmann::detail::exception& failure) override {
                return reject("Malformed JSON at byte " + std::to_string(position) + ": " + failure.what());
            }
        };

        std::expected<D::Draft, DefinitionError> decode(std::string_view bytes, std::string origin, std::size_t choice_budget) {
            if (bytes.size() > maximum_definition_bytes)
                return std::unexpected(DefinitionError{std::move(origin), "$", "Definition exceeds 65536 bytes", {}, {}, {}});
            if (bytes.find('\0') != std::string_view::npos)
                return std::unexpected(DefinitionError{std::move(origin), "$", "Raw NUL in definition", {}, {}, {}});
            DefinitionSax parser(std::move(origin), choice_budget);
            if (!nlohmann::json::sax_parse(bytes.begin(), bytes.end(), &parser) || !parser.complete)
                return std::unexpected(std::move(parser.error));
            return std::move(parser.draft);
        }
        std::string_view describe_code(D::DiagnosticCode code) {
            switch (code) {
                case D::DiagnosticCode::EmptyIdentity:
                    return "empty identity";
                case D::DiagnosticCode::DuplicateIdentity:
                    return "duplicate identity";
                case D::DiagnosticCode::InvalidRevision:
                    return "invalid revision";
                case D::DiagnosticCode::MissingReference:
                    return "missing reference";
                case D::DiagnosticCode::UnsupportedOperation:
                    return "unsupported application operation";
                case D::DiagnosticCode::InvalidNodeShape:
                    return "invalid node shape";
            }
            return "invalid authored definition";
        }
    }

    std::expected<D::Draft, DefinitionError> decode_definition(std::string_view bytes, std::string origin) {
        return decode(bytes, std::move(origin), maximum_definition_choices);
    }

    std::expected<D::DefinitionHandle, DefinitionError> compile_definition(std::string_view bytes, std::string origin) {
        auto draft = decode_definition(bytes, origin);
        if (!draft)
            return std::unexpected(std::move(draft.error()));
        auto compiled = D::compile(*draft, operation_keys());
        if (!compiled)
            return std::unexpected(
                DefinitionError{std::move(origin), "$", "Definition compilation rejected", {}, {}, std::move(compiled.error())}
            );
        return std::move(*compiled);
    }

    std::expected<D::DefinitionHandle, DefinitionError> load_definition(const std::string& path) {
        const auto failure = [&](std::string message) -> std::expected<D::DefinitionHandle, DefinitionError> {
            return std::unexpected(DefinitionError{path, "$", std::move(message), {}, {}, {}});
        };
        if (path.empty() || path.find('\0') != std::string::npos)
            return failure("Definition path is empty or contains NUL");
        std::string bytes(maximum_definition_bytes + 1, '\0');
        auto* stream = std::fopen(path.c_str(), "rb");
        if (!stream)
            return failure("Cannot read definition: " + std::error_code(errno, std::generic_category()).message());
        const auto count = std::fread(bytes.data(), 1, bytes.size(), stream);
        const bool failed = std::ferror(stream);
        const int read_error = errno;
        const int closed = std::fclose(stream);
        const int close_error = errno;
        if (failed)
            return failure("Definition read failed: " + std::error_code(read_error, std::generic_category()).message());
        if (closed)
            return failure("Definition close failed: " + std::error_code(close_error, std::generic_category()).message());
        if (count > maximum_definition_bytes)
            return failure("Definition exceeds 65536 bytes");
        bytes.resize(count);
        return compile_definition(bytes, path);
    }

    std::string format_definition_error(const DefinitionError& error) {
        std::string result = error.origin + ": " + error.field + ": " + error.message;
        for (const auto& diagnostic : error.diagnostics) {
            result += '\n' + error.origin + ": " + location(diagnostic.node_index, diagnostic.choice_index, diagnostic.field) + ": " +
                      std::string(describe_code(diagnostic.code)) + " [" + diagnostic.definition.value + "@" +
                      std::to_string(diagnostic.revision) + "] value='" + diagnostic.value + "'";
        }
        return result;
    }

    namespace Testing {
        std::expected<D::Draft, DefinitionError>
        decode_definition_choice_budget(std::string_view bytes, std::string origin, std::size_t budget) {
            return decode(bytes, std::move(origin), std::min(budget, maximum_definition_choices));
        }
    }
}
