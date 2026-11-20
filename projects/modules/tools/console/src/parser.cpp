#include <cheryl/tools/console/parser.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <new>

namespace CE::Tools::Console {
    namespace {
        Failure at(Code code, const Token& token) { return {code, token.offset, token.length}; }
        bool space(char byte) { return byte == ' ' || byte == '\t'; }
        template<class Number> Checked<Number> number(std::string_view text, const Token& token) {
            Number result{};
            const auto converted = std::from_chars(text.data(), text.data() + text.size(), result);
            if (text.empty() || converted.ec == std::errc::result_out_of_range)
                return std::unexpected(at(Code::Range, token));
            if (converted.ec != std::errc{} || converted.ptr != text.data() + text.size())
                return std::unexpected(at(Code::Type, token));
            return result;
        }
        Checked<Value> convert(std::string_view text, const Schema& schema, const Context& context, const Token& token) {
            Value value;
            switch (schema.type) {
            case Type::Boolean:
                if (text != "true" && text != "false") return std::unexpected(at(Code::Type, token));
                value = Value::boolean(text == "true"); break;
            case Type::Unsigned: {
                const auto checked = number<std::uint64_t>(text, token);
                if (!checked) return std::unexpected(checked.error());
                value = Value::unsigned_integer(*checked); break;
            }
            case Type::Integer: {
                const auto checked = number<std::int64_t>(text, token);
                if (!checked) return std::unexpected(checked.error());
                value = Value::integer(*checked); break;
            }
            case Type::Scalar: {
                if (!text.ends_with(schema.unit)) return std::unexpected(at(Code::Type, token));
                text.remove_suffix(schema.unit.size());
                const auto checked = number<double>(text, token);
                if (!checked) return std::unexpected(checked.error());
                value = Value::scalar(*checked); break;
            }
            case Type::Text: value = Value::text(std::string(text)); break;
            case Type::Enumeration: {
                const auto found = std::ranges::find(schema.choices, text);
                if (found == schema.choices.end()) return std::unexpected(at(Code::Range, token));
                value = Value::enumeration(static_cast<std::uint32_t>(found - schema.choices.begin())); break;
            }
            case Type::Handle: {
                auto handle = context.resolve(text, schema.domain);
                if (!handle) return std::unexpected(at(handle.error().code, token));
                value = Value::handle(std::move(*handle)); break;
            }
            default: return std::unexpected(at(Code::Unsupported, token));
            }
            auto checked = validate(value, schema, context);
            if (!checked) return std::unexpected(at(checked.error().code, token));
            return value;
        }
        Checked<std::size_t> argument_index(const Descriptor& descriptor, const Token& token, std::size_t positional) {
            if (!token.equals) {
                if (positional < descriptor.arguments.size()) return positional;
            } else {
                const auto name = std::string_view(token.text).substr(0, *token.equals);
                for (std::size_t index = 0; index < descriptor.arguments.size(); ++index)
                    if (descriptor.arguments[index].name == name) return index;
            }
            return std::unexpected(at(Code::Arguments, token));
        }
    }
    Checked<Parsed> parse(std::string_view line, bool partial) {
        try {
            if (line.size() > max_line) return std::unexpected(Failure{Code::Limit, max_line, line.size() - max_line});
            if (!valid_utf8(line)) return std::unexpected(Failure{Code::Encoding, 0, line.size()});
            Parsed parsed;
            std::size_t cursor = 0;
            while (cursor < line.size()) {
                if (space(line[cursor])) { ++cursor; parsed.trailing_space = true; continue; }
                parsed.trailing_space = false;
                if (parsed.tokens.size() == max_tokens) return std::unexpected(Failure{Code::Limit, cursor, 1});
                Token token;
                token.offset = cursor;
                bool quoted = false, escaping = false;
                while (cursor < line.size()) {
                    const auto byte = line[cursor];
                    if (!quoted && !escaping && space(byte)) break;
                    ++cursor;
                    if (escaping) {
                        if (byte != '"' && byte != '\\') return std::unexpected(Failure{Code::Syntax, cursor - 1, 1});
                        token.text += byte; escaping = false;
                    } else if (byte == '\\' && quoted) escaping = true;
                    else if (byte == '"') quoted = !quoted;
                    else {
                        if (byte == '=' && !quoted) {
                            if (token.equals) return std::unexpected(Failure{Code::Syntax, cursor - 1, 1});
                            token.equals = token.text.size();
                        }
                        token.text += byte;
                    }
                    if (token.text.size() > max_text) return std::unexpected(Failure{Code::Limit, token.offset, cursor - token.offset});
                }
                token.length = cursor - token.offset;
                token.complete = !quoted && !escaping;
                if (!partial && !token.complete) return std::unexpected(at(Code::Syntax, token));
                parsed.tokens.push_back(std::move(token));
            }
            return parsed;
        } catch (const std::bad_alloc&) { return std::unexpected(Failure{Code::Allocation}); }
    }
    Checked<BoundRequest> bind_text(const Registry& registry, const Context& context, std::string_view line) {
        try {
            auto parsed = parse(line);
            if (!parsed) return std::unexpected(parsed.error());
            if (parsed->tokens.empty()) return std::unexpected(Failure{Code::Syntax});
            const auto descriptor = registry.describe(parsed->tokens[0].text, context);
            if (!descriptor) return std::unexpected(at(descriptor.error().code, parsed->tokens[0]));
            if (parsed->tokens.size() != descriptor->arguments.size() + 1)
                return std::unexpected(Failure{Code::Arguments, line.size(), 0});
            std::vector<Value> arguments(descriptor->arguments.size());
            std::vector<bool> seen(arguments.size(), false);
            bool named = false;
            for (std::size_t index = 1; index < parsed->tokens.size(); ++index) {
                const auto& token = parsed->tokens[index];
                if (named && !token.equals) return std::unexpected(at(Code::Arguments, token));
                named = named || token.equals.has_value();
                const auto position = argument_index(*descriptor, token, index - 1);
                if (!position) return std::unexpected(position.error());
                if (seen[*position]) return std::unexpected(at(Code::Arguments, token));
                auto text = std::string_view(token.text);
                if (token.equals) text.remove_prefix(*token.equals + 1);
                auto value = convert(text, descriptor->arguments[*position].schema, context, token);
                if (!value) return std::unexpected(value.error());
                seen[*position] = true;
                arguments[*position] = std::move(*value);
            }
            return registry.bind(descriptor->name, std::move(arguments), context);
        } catch (const std::bad_alloc&) { return std::unexpected(Failure{Code::Allocation}); }
    }
    Checked<std::vector<std::string>> complete(const Registry& registry, const Context& context, std::string_view line) {
        try {
            auto parsed = parse(line, true);
            if (!parsed) return std::unexpected(parsed.error());
            if (!context.live()) return std::unexpected(Failure{Code::Expired});
            std::vector<std::string> candidates;
            std::string prefix;
            if (parsed->tokens.empty() || (parsed->tokens.size() == 1 && !parsed->trailing_space)) {
                if (!parsed->tokens.empty()) prefix = parsed->tokens.front().text;
                for (const auto& descriptor : registry.commands(context)) candidates.push_back(descriptor.name);
            } else {
                const auto descriptor = registry.describe(parsed->tokens.front().text, context);
                if (!descriptor) return candidates;
                const auto completed = parsed->tokens.size() - (parsed->trailing_space ? 1 : 2);
                std::vector<bool> seen(descriptor->arguments.size(), false);
                bool named = false;
                for (std::size_t index = 0; index < completed; ++index) {
                    const auto& token = parsed->tokens[index + 1];
                    if (named && !token.equals) return std::unexpected(at(Code::Arguments, token));
                    named = named || token.equals.has_value();
                    const auto position = argument_index(*descriptor, token, index);
                    if (!position) return std::unexpected(position.error());
                    if (seen[*position]) return std::unexpected(at(Code::Arguments, token));
                    auto text = std::string_view(token.text);
                    if (token.equals) text.remove_prefix(*token.equals + 1);
                    const auto checked = convert(text, descriptor->arguments[*position].schema, context, token);
                    if (!checked) return std::unexpected(checked.error());
                    seen[*position] = true;
                }
                Token current;
                if (!parsed->trailing_space) current = parsed->tokens.back();
                prefix = current.text;
                if (completed >= descriptor->arguments.size()) return candidates;
                auto position = argument_index(*descriptor, current, completed);
                if (current.equals) {
                    if (!position) return candidates;
                    prefix.erase(0, *current.equals + 1);
                } else if (named) {
                    for (std::size_t index = 0; index < seen.size(); ++index)
                        if (!seen[index]) candidates.push_back(descriptor->arguments[index].name + "=");
                    position = std::unexpected(Failure{Code::Arguments});
                }
                if (position && !seen[*position]) {
                    const auto& schema = descriptor->arguments[*position].schema;
                    if (schema.type == Type::Enumeration) candidates = schema.choices;
                    if (schema.type == Type::Handle) candidates = context.aliases(schema.domain);
                    if (schema.type == Type::Boolean) candidates = {"false", "true"};
                    if (!current.equals) for (std::size_t index = 0; index < seen.size(); ++index)
                        if (!seen[index]) candidates.push_back(descriptor->arguments[index].name + "=");
                }
            }
            std::erase_if(candidates, [&](const auto& candidate) { return !candidate.starts_with(prefix); });
            std::ranges::sort(candidates);
            candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
            return candidates;
        } catch (const std::bad_alloc&) { return std::unexpected(Failure{Code::Allocation}); }
    }
}
