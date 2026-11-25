#include "scenario-internal.h"

#include <algorithm>
#include <charconv>
#include <new>

namespace CE::Tools::Console::Scenario {
    namespace {
        Error error(Code code, std::size_t line) { return {line, {code}}; }
        bool identifier(std::string_view text) {
            if (text.empty() || text.size() > 64) return false;
            return std::ranges::all_of(text, [](char byte) {
                return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
                    (byte >= '0' && byte <= '9') || byte == '_';
            });
        }
        template<class Number> Checked<Number> number(std::string_view text, std::size_t line) {
            Number value{};
            auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
            if (text.empty() || parsed.ec == std::errc::result_out_of_range) return std::unexpected(error(Code::Range, line));
            if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) return std::unexpected(error(Code::Type, line));
            return value;
        }
        Checked<std::size_t> add_name(Detail::PlanData& plan, const Token& token, const Schema& schema,
                                      std::size_t line, Stage expected, Code code, Route route) {
            if (!identifier(token.text)) return std::unexpected(error(Code::Syntax, line));
            if (plan.names.size() == max_names) return std::unexpected(error(Code::Limit, line));
            for (const auto& name : plan.names) if (name.text == token.text) return std::unexpected(error(Code::Duplicate, line));
            plan.names.push_back({token.text, schema, line, expected, code, route});
            return plan.names.size() - 1;
        }
    }
    namespace Detail {
        bool fresh_channels(const PlanData& plan) noexcept {
            if (plan.started || !plan.context.live() || !plan.gameplay_session || !plan.gameplay_session->accepting() ||
                plan.gameplay_session->retained() ||
                !plan.context.same_scope(plan.gameplay_session->context())) return false;
            if (!plan.diagnostic) return !plan.diagnostic_session;
            return plan.diagnostic->live() && plan.diagnostic_session &&
                plan.diagnostic_session != plan.gameplay_session && !plan.context.same_scope(*plan.diagnostic) &&
                plan.diagnostic_session->accepting() && !plan.diagnostic_session->retained() &&
                plan.diagnostic->same_scope(plan.diagnostic_session->context());
        }
        bool has_handle(const Schema& schema) {
            return schema.type == Type::Handle || std::ranges::any_of(schema.children, has_handle);
        }
        Checked<Value> literal(std::string_view text, const Schema& schema, const Context& context, std::size_t line) {
            Value result;
            switch (schema.type) {
            case Type::Boolean:
                if (text != "true" && text != "false") return std::unexpected(error(Code::Type, line));
                result = Value::boolean(text == "true"); break;
            case Type::Unsigned: {
                auto parsed = number<std::uint64_t>(text, line); if (!parsed) return std::unexpected(parsed.error());
                result = Value::unsigned_integer(*parsed); break;
            }
            case Type::Integer: {
                auto parsed = number<std::int64_t>(text, line); if (!parsed) return std::unexpected(parsed.error());
                result = Value::integer(*parsed); break;
            }
            case Type::Scalar: {
                if (!text.ends_with(schema.unit)) return std::unexpected(error(Code::Type, line));
                text.remove_suffix(schema.unit.size());
                auto parsed = number<double>(text, line); if (!parsed) return std::unexpected(parsed.error());
                result = Value::scalar(*parsed); break;
            }
            case Type::Text: result = Value::text(std::string(text)); break;
            case Type::Enumeration: {
                const auto found = std::ranges::find(schema.choices, text);
                if (found == schema.choices.end()) return std::unexpected(error(Code::Range, line));
                result = Value::enumeration(static_cast<std::uint32_t>(found - schema.choices.begin())); break;
            }
            case Type::Handle: {
                auto handle = context.resolve(text, schema.domain);
                if (!handle) return std::unexpected(Error{line, handle.error()});
                result = Value::handle(*handle); break;
            }
            default: return std::unexpected(error(Code::Unsupported, line));
            }
            auto checked = validate(result, schema, context);
            if (!checked) return std::unexpected(Error{line, checked.error()});
            return result;
        }
        Checked<Reference> reference(const PlanData& plan, std::string_view text, std::size_t line) {
            if (!text.starts_with('$')) return std::unexpected(error(Code::Syntax, line));
            text.remove_prefix(1);
            const auto dot = text.find('.');
            const auto name = text.substr(0, dot);
            auto found = std::ranges::find(plan.names, name, &Name::text);
            if (found == plan.names.end()) return std::unexpected(error(Code::Arguments, line));
            Reference result{static_cast<std::size_t>(found - plan.names.begin()), {}, found->schema};
            if (dot != std::string_view::npos) {
                text.remove_prefix(dot + 1);
                while (true) {
                    const auto next = text.find('.');
                    const auto field = text.substr(0, next);
                    if (result.schema.type != Type::Record || field.empty()) return std::unexpected(error(Code::Schema, line));
                    const auto position = std::ranges::find(result.schema.fields, field);
                    if (position == result.schema.fields.end()) return std::unexpected(error(Code::Schema, line));
                    const auto index = static_cast<std::size_t>(position - result.schema.fields.begin());
                    result.fields.push_back(index);
                    auto child = result.schema.children[index];
                    result.schema = std::move(child);
                    if (next == std::string_view::npos) break;
                    text.remove_prefix(next + 1);
                }
            }
            return result;
        }
        const Value* value(const NamedResult& named, const Reference& ref) noexcept {
            if (!named.available) return nullptr;
            const auto* current = &named.result.value;
            for (auto index : ref.fields) {
                const auto* children = std::get_if<Value::Children>(&current->data);
                if (!children || index >= children->size()) return nullptr;
                current = &(*children)[index];
            }
            return current;
        }
    }
    std::string_view Plan::fixture() const noexcept { return data_->fixture; }
    std::size_t Plan::assertions() const noexcept { return data_->assertions; }
    static Checked<std::shared_ptr<const Detail::PlanData>> prepare_data(
            const Registry& registry, std::shared_ptr<Detail::PlanData> plan, std::string_view source) {
        std::size_t line_number = 0;
        try {
            if (source.size() > max_file_bytes) return std::unexpected(error(Code::Limit, 0));
            if (!valid_utf8(source)) return std::unexpected(error(Code::Encoding, 0));
            bool version = false, finished = false;
            while (!source.empty()) {
                if (++line_number > max_lines) return std::unexpected(error(Code::Limit, line_number));
                const auto end = source.find('\n');
                auto line = source.substr(0, end);
                source = end == std::string_view::npos ? std::string_view{} : source.substr(end + 1);
                if (line.ends_with('\r')) line.remove_suffix(1);
                const auto first = line.find_first_not_of(" \t");
                if (first == std::string_view::npos || line[first] == '#') continue;
                auto parsed = parse(line);
                if (!parsed) return std::unexpected(Error{line_number, parsed.error()});
                auto& tokens = parsed->tokens;
                if (finished) return std::unexpected(error(Code::Syntax, line_number));
                if (!version) {
                    if (tokens.size() != 2 || tokens[0].text != "script.version" || tokens[1].text != "1")
                        return std::unexpected(error(Code::Unsupported, line_number));
                    version = true; continue;
                }
                if (plan->fixture.empty()) {
                    if (tokens.size() != 2 || tokens[0].text != "fixture.load" || tokens[1].text.empty())
                        return std::unexpected(error(Code::Syntax, line_number));
                    plan->fixture = tokens[1].text; continue;
                }
                if (plan->statements.size() == max_statements) return std::unexpected(error(Code::Limit, line_number));
                Detail::Statement statement;
                statement.line = line_number;
                const auto& directive = tokens[0].text;
                if (directive == "finish") {
                    if (tokens.size() != 1) return std::unexpected(error(Code::Syntax, line_number));
                    statement.kind = Detail::Kind::Finish; finished = true;
                } else if (directive == "expect") {
                    if (tokens.size() != 4 || tokens[2].text != "==") return std::unexpected(error(Code::Syntax, line_number));
                    auto ref = Detail::reference(*plan, tokens[1].text, line_number);
                    if (!ref) return std::unexpected(ref.error());
                    auto literal = Detail::literal(tokens[3].text, ref->schema,
                        plan->selected(plan->names[ref->name].route), line_number);
                    if (!literal) return std::unexpected(literal.error());
                    statement.kind = Detail::Kind::Expect;
                    statement.reference = std::move(*ref); statement.literal = std::move(*literal);
                    statement.spelling = tokens[1].text; ++plan->assertions;
                } else if (directive == "await" || directive == "retry") {
                    const bool awaiting = directive == "await";
                    if (tokens.size() != (awaiting ? 5u : 4u) || tokens[tokens.size() - 2].text != "->")
                        return std::unexpected(error(Code::Syntax, line_number));
                    auto ref = Detail::reference(*plan, tokens[1].text, line_number);
                    if (!ref) return std::unexpected(ref.error());
                    if (!ref->fields.empty()) return std::unexpected(error(Code::Type, line_number));
                    if (awaiting) {
                        if (!tokens[2].text.starts_with("max_steps=")) return std::unexpected(error(Code::Syntax, line_number));
                        auto budget = number<std::size_t>(std::string_view(tokens[2].text).substr(10), line_number);
                        if (!budget) return std::unexpected(budget.error());
                        if (*budget > max_steps) return std::unexpected(error(Code::Limit, line_number));
                        statement.budget = *budget;
                    }
                    statement.kind = awaiting ? Detail::Kind::Await : Detail::Kind::Retry;
                    statement.source = ref->name;
                    const auto& prior = plan->names[ref->name];
                    statement.route = prior.route;
                    auto name = add_name(*plan, tokens.back(), ref->schema, line_number, prior.expected, prior.error, prior.route);
                    if (!name) return std::unexpected(name.error());
                    statement.output = *name;
                } else {
                    std::size_t begin = 0;
                    if (plan->routed && directive == "observe") {
                        if (!plan->diagnostic) return std::unexpected(error(Code::Unauthorized, line_number));
                        statement.route = Route::Diagnostic; begin = 1;
                    }
                    if (begin == tokens.size()) return std::unexpected(error(Code::Syntax, line_number));
                    if (tokens[begin].text == "refuse") { ++begin; statement.expected = Stage::Refused; }
                    else if (tokens[begin].text == "error") {
                        if (tokens.size() < begin + 3 || tokens[begin + 1].text != "Owner")
                            return std::unexpected(error(Code::Unsupported, line_number));
                        begin += 2; statement.expected = Stage::Failed; statement.expected_error = Code::Owner;
                    }
                    if (tokens.size() < begin + 3 || tokens[tokens.size() - 2].text != "->")
                        return std::unexpected(error(Code::Syntax, line_number));
                    if (tokens[begin].text == "test.run") return std::unexpected(error(Code::Unsupported, line_number));
                    const auto& context = plan->selected(statement.route);
                    auto resolved = registry.resolve(tokens[begin].text, context);
                    if (!resolved) return std::unexpected(Error{line_number, resolved.error()});
                    const auto* descriptor = resolved->descriptor();
                    if (statement.route == Route::Diagnostic && descriptor->kind != ::CE::Tools::Console::Kind::Query)
                        return std::unexpected(error(Code::Unauthorized, line_number));
                    if (tokens.size() - begin - 3 != descriptor->arguments.size()) return std::unexpected(error(Code::Arguments, line_number));
                    statement.descriptor = *descriptor;
                    statement.inputs.resize(descriptor->arguments.size());
                    std::vector<bool> seen(descriptor->arguments.size(), false);
                    bool named = false, dynamic = false;
                    for (std::size_t token_index = begin + 1; token_index < tokens.size() - 2; ++token_index) {
                        const auto& token = tokens[token_index];
                        if (named && !token.equals) return std::unexpected(error(Code::Arguments, line_number));
                        named = named || token.equals.has_value();
                        std::size_t index = token_index - begin - 1;
                        std::string_view text = token.text;
                        if (token.equals) {
                            auto found = std::ranges::find(descriptor->arguments, text.substr(0, *token.equals), &Argument::name);
                            if (found == descriptor->arguments.end()) return std::unexpected(error(Code::Arguments, line_number));
                            index = static_cast<std::size_t>(found - descriptor->arguments.begin());
                            text.remove_prefix(*token.equals + 1);
                        }
                        if (seen[index]) return std::unexpected(error(Code::Arguments, line_number));
                        seen[index] = true;
                        const auto& schema = descriptor->arguments[index].schema;
                        if (text.starts_with('$')) {
                            auto ref = Detail::reference(*plan, text, line_number);
                            if (!ref) return std::unexpected(ref.error());
                            if (ref->schema != schema) return std::unexpected(error(Code::Type, line_number));
                            if (plan->routed && plan->names[ref->name].route != statement.route && Detail::has_handle(schema))
                                return std::unexpected(error(Code::Unauthorized, line_number));
                            statement.inputs[index].reference = std::move(*ref); dynamic = true;
                        } else {
                            auto literal = Detail::literal(text, schema, context, line_number);
                            if (!literal) return std::unexpected(literal.error());
                            statement.inputs[index].literal = std::move(*literal);
                        }
                    }
                    if (plan->routed) {
                        statement.resolved = std::move(*resolved);
                        if (!dynamic) {
                            std::vector<Value> arguments;
                            for (const auto& input : statement.inputs) arguments.push_back(input.literal);
                            auto bound = registry.bind(*statement.resolved, std::move(arguments));
                            if (!bound) return std::unexpected(Error{line_number, bound.error()});
                            statement.bound = std::move(*bound);
                        }
                    } else if (!dynamic) {
                        const auto start = tokens[begin].offset;
                        const auto stop = tokens[tokens.size() - 2].offset;
                        auto bound = bind_text(registry, context, line.substr(start, stop - start));
                        if (!bound) return std::unexpected(Error{line_number, bound.error()});
                        statement.bound = std::move(*bound);
                    }
                    auto name = add_name(*plan, tokens.back(), descriptor->result, line_number,
                        statement.expected, statement.expected_error, statement.route);
                    if (!name) return std::unexpected(name.error());
                    statement.output = *name;
                }
                plan->statements.push_back(std::move(statement));
            }
            if (!finished || !plan->assertions) return std::unexpected(error(Code::Syntax, line_number));
            return std::shared_ptr<const Detail::PlanData>(std::move(plan));
        } catch (const std::bad_alloc&) { return std::unexpected(error(Code::Allocation, line_number)); }
    }
    Checked<Plan> prepare(const Registry& registry, const Context& context, std::string_view source) {
        try {
            auto data = std::make_shared<Detail::PlanData>(); data->context = context;
            auto parsed = prepare_data(registry, std::move(data), source);
            if (!parsed) return std::unexpected(parsed.error());
            return Plan(std::move(*parsed));
        } catch (const std::bad_alloc&) { return std::unexpected(error(Code::Allocation, 0)); }
    }
    Checked<Plan> prepare(const Registry& registry, const Channels& channels, std::string_view source) {
        try {
            auto data = std::make_shared<Detail::PlanData>();
            data->context = channels.gameplay.context; data->gameplay_session = &channels.gameplay.session;
            data->routed = true; data->registry = &registry;
            if (channels.diagnostic) {
                data->diagnostic = channels.diagnostic->context;
                data->diagnostic_session = &channels.diagnostic->session;
            }
            if (!Detail::fresh_channels(*data)) return std::unexpected(error(Code::Unauthorized, 0));
            auto parsed = prepare_data(registry, std::move(data), source);
            if (!parsed) return std::unexpected(parsed.error());
            return Plan(std::move(*parsed));
        } catch (const std::bad_alloc&) { return std::unexpected(error(Code::Allocation, 0)); }
    }
}
