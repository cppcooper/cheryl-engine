#include "codec.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <utility>

namespace GateConsole {
    namespace {
        using Json = nlohmann::json;
        enum class Object { Envelope, Flags, Dialogue };
        enum class Field : unsigned {
            None,
            SaveSchema,
            Flags,
            Dialogue,
            Introduced,
            SnapshotSchema,
            Definition,
            Revision,
            Session,
            Node,
            Step
        };
        constexpr unsigned bit(Field field) {
            return 1U << static_cast<unsigned>(field);
        }
        constexpr unsigned fields(Object object) {
            switch (object) {
                case Object::Envelope:
                    return bit(Field::SaveSchema) | bit(Field::Flags) | bit(Field::Dialogue);
                case Object::Flags:
                    return bit(Field::Introduced);
                case Object::Dialogue:
                    return bit(Field::SnapshotSchema) | bit(Field::Definition) | bit(Field::Revision) | bit(Field::Session) |
                           bit(Field::Node) | bit(Field::Step);
            }
            return 0;
        }
        Field field_for(Object object, std::string_view key) {
            if (object == Object::Envelope) {
                if (key == "save_schema")
                    return Field::SaveSchema;
                if (key == "flags")
                    return Field::Flags;
                if (key == "dialogue")
                    return Field::Dialogue;
            } else if (object == Object::Flags) {
                if (key == "introduced")
                    return Field::Introduced;
            } else {
                if (key == "snapshot_schema")
                    return Field::SnapshotSchema;
                if (key == "definition")
                    return Field::Definition;
                if (key == "definition_revision")
                    return Field::Revision;
                if (key == "session")
                    return Field::Session;
                if (key == "node")
                    return Field::Node;
                if (key == "step")
                    return Field::Step;
            }
            return Field::None;
        }

        // A two-frame SAX decoder, not an arbitrary JSON DOM. Decoded property
        // names are checked before replacement; even escaped aliases duplicate.
        struct EnvelopeSax final : nlohmann::json_sax<Json> {
            struct Frame {
                Object object;
                unsigned seen = 0;
                Field pending = Field::None;
            };
            std::array<Frame, 2> frames{};
            std::size_t depth = 0;
            bool started = false;
            bool complete = false;
            PairedState state;
            std::string error;

            bool reject(std::string message) {
                if (error.empty())
                    error = std::move(message);
                return false;
            }
            Field pending() const { return depth ? frames[depth - 1].pending : Field::None; }
            bool consumed() {
                frames[depth - 1].pending = Field::None;
                return true;
            }
            bool start_object(std::size_t) override {
                if (!started) {
                    started = true;
                    frames[depth++] = {Object::Envelope};
                    return true;
                }
                if (depth != 1 || (pending() != Field::Flags && pending() != Field::Dialogue))
                    return reject("Unexpected object or excessive nesting");
                const auto object = pending() == Field::Flags ? Object::Flags : Object::Dialogue;
                consumed();
                frames[depth++] = {object};
                return true;
            }
            bool key(string_t& key) override {
                if (!depth)
                    return reject("Unexpected property");
                auto& frame = frames[depth - 1];
                const auto field = field_for(frame.object, key);
                if (field == Field::None)
                    return reject("Unknown property: " + key);
                if (frame.seen & bit(field))
                    return reject("Duplicate property: " + key);
                frame.seen |= bit(field);
                frame.pending = field;
                return true;
            }
            bool end_object() override {
                if (!depth || frames[depth - 1].seen != fields(frames[depth - 1].object))
                    return reject("Missing required property");
                --depth;
                if (!depth)
                    complete = true;
                return true;
            }
            bool boolean(bool value) override {
                if (pending() != Field::Introduced)
                    return reject("Expected an object, string or integer, not a boolean");
                state.introduced = value;
                return consumed();
            }
            bool number_unsigned(number_unsigned_t value) override {
                switch (pending()) {
                    case Field::SaveSchema:
                    case Field::SnapshotSchema:
                        if (value != 1)
                            return reject("Unsupported schema");
                        break;
                    case Field::Revision:
                        if (!value)
                            return reject("Definition revision must be positive");
                        state.dialogue.definition_revision = value;
                        break;
                    case Field::Step:
                        state.dialogue.step = value;
                        break;
                    default:
                        return reject("Unexpected integer; no type coercion is allowed");
                }
                return consumed();
            }
            bool number_integer(number_integer_t value) override {
                if (value < 0)
                    return reject("Negative integer is out of range");
                return number_unsigned(static_cast<number_unsigned_t>(value));
            }
            bool string(string_t& value) override {
                if (value.empty())
                    return reject("IDs must be nonempty");
                switch (pending()) {
                    case Field::Definition:
                        state.dialogue.definition.value = std::move(value);
                        break;
                    case Field::Session:
                        state.dialogue.session.value = std::move(value);
                        break;
                    case Field::Node:
                        state.dialogue.node.value = std::move(value);
                        break;
                    default:
                        return reject("Unexpected string; no type coercion is allowed");
                }
                return consumed();
            }
            bool null() override { return reject("Null is not a save field type"); }
            bool number_float(number_float_t, const string_t&) override { return reject("Expected an in-range integer, not a float"); }
            bool binary(binary_t&) override { return reject("Binary data is not supported"); }
            bool start_array(std::size_t) override { return reject("Arrays are not save objects"); }
            bool end_array() override { return reject("Unexpected array"); }
            bool parse_error(std::size_t position, const std::string&, const nlohmann::detail::exception& failure) override {
                return reject("Malformed JSON at byte " + std::to_string(position) + ": " + failure.what());
            }
        };
    }

    std::string encode(const PairedState& state) {
        const auto& snapshot = state.dialogue;
        const Json document{
            {"save_schema", 1},
            {"flags", {{"introduced", state.introduced}}},
            {"dialogue",
             {{"snapshot_schema", snapshot.schema},
              {"definition", snapshot.definition.value},
              {"definition_revision", snapshot.definition_revision},
              {"session", snapshot.session.value},
              {"node", snapshot.node.value},
              {"step", snapshot.step}}}
        };
        return document.dump(2) + '\n';
    }

    std::expected<PairedState, std::string> decode(std::string_view bytes) {
        if (bytes.size() > maximum_save_bytes)
            return std::unexpected("Save exceeds 64 KiB");
        // Check all bytes, including suffixes the pinned parser may treat as EOF.
        if (bytes.find('\0') != std::string_view::npos)
            return std::unexpected("Raw NUL in save");
        EnvelopeSax parser;
        if (!Json::sax_parse(bytes.begin(), bytes.end(), &parser) || !parser.complete)
            return std::unexpected(parser.error.empty() ? "Incomplete save envelope" : std::move(parser.error));
        return std::move(parser.state);
    }

    std::expected<void, std::string> restore_bytes(Encounter& encounter, std::string_view bytes) {
        auto decoded = decode(bytes);
        if (!decoded)
            return std::unexpected(std::move(decoded.error()));
        const auto restored = encounter.restore(*decoded);
        if (!restored)
            return std::unexpected(std::string(describe(restored.error())));
        return {};
    }
}
