#include <cheryl/tools/console/registry.h>
#include "internal.h"

#include <algorithm>
#include <cmath>
#include <new>

namespace CE::Tools::Console {
    Work::~Work() = default;

    namespace {
        Checked<void> fail(Code code) { return std::unexpected(Failure{code}); }
        bool name_valid(std::string_view name) {
            return !name.empty() && name.size() <= 64 && std::ranges::all_of(name, [](unsigned char byte) {
                return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
                    (byte >= '0' && byte <= '9') || byte == '.' || byte == '_' || byte == '-';
            });
        }
        Checked<void> schema_valid(const Schema& schema, std::size_t depth, std::size_t& nodes) {
            if (depth > max_depth || ++nodes > max_nodes || schema.choices.size() > max_commands ||
                schema.fields.size() > max_arguments || schema.children.size() > max_arguments ||
                schema.count > max_arguments || schema.text_limit > max_output || schema.unit.size() > 64 || schema.domain.size() > 64)
                return fail(Code::Limit);
            if (schema.integer_min > schema.integer_max || !valid_utf8(schema.unit))
                return fail(Code::Schema);
            if ((schema.type != Type::Scalar && !schema.unit.empty()) ||
                (schema.type != Type::Handle && !schema.domain.empty())) return fail(Code::Schema);
            switch (schema.type) {
            case Type::Record:
                if (schema.fields.size() != schema.children.size()) return fail(Code::Schema);
                break;
            case Type::List:
                if (schema.children.size() != 1 || !schema.fields.empty()) return fail(Code::Schema);
                break;
            case Type::Handle:
                if (!name_valid(schema.domain)) return fail(Code::Schema);
                break;
            case Type::Enumeration:
                if (schema.choices.empty()) return fail(Code::Schema);
                break;
            case Type::Boolean: case Type::Unsigned: case Type::Integer: case Type::Text: break;
            case Type::Scalar:
                if (!name_valid(schema.unit)) return fail(Code::Schema);
                break;
            default: return fail(Code::Unsupported);
            }
            if (schema.type != Type::Record && schema.type != Type::List &&
                (!schema.children.empty() || !schema.fields.empty())) return fail(Code::Schema);
            if (schema.type != Type::Enumeration && !schema.choices.empty()) return fail(Code::Schema);
            for (const auto* names : {&schema.fields, &schema.choices}) {
                for (std::size_t index = 0; index < names->size(); ++index) {
                    if (!name_valid((*names)[index])) return fail(Code::Schema);
                    for (std::size_t previous = 0; previous < index; ++previous)
                        if ((*names)[previous] == (*names)[index]) return fail(Code::Schema);
                }
            }
            for (const auto& child : schema.children) {
                auto checked = schema_valid(child, depth + 1, nodes);
                if (!checked) return checked;
            }
            return {};
        }
        Checked<void> value_valid(const Value& value, const Schema& schema, const Context& context,
                                  std::size_t depth, std::size_t& nodes, std::size_t& bytes) {
            if (depth > max_depth || ++nodes > max_nodes) return fail(Code::Limit);
            if (value.type != schema.type) return fail(Code::Type);
            switch (schema.type) {
            case Type::Boolean: if (!std::holds_alternative<bool>(value.data)) return fail(Code::Type); break;
            case Type::Unsigned:
                if (!std::holds_alternative<std::uint64_t>(value.data)) return fail(Code::Type);
                if (std::get<std::uint64_t>(value.data) > schema.unsigned_max) return fail(Code::Range);
                break;
            case Type::Integer:
                if (!std::holds_alternative<std::int64_t>(value.data)) return fail(Code::Type);
                if (std::get<std::int64_t>(value.data) < schema.integer_min ||
                    std::get<std::int64_t>(value.data) > schema.integer_max) return fail(Code::Range);
                break;
            case Type::Scalar:
                if (!std::holds_alternative<double>(value.data)) return fail(Code::Type);
                if (!std::isfinite(std::get<double>(value.data))) return fail(Code::Range);
                break;
            case Type::Text:
                if (!std::holds_alternative<std::string>(value.data)) return fail(Code::Type);
                if (std::get<std::string>(value.data).size() > schema.text_limit) return fail(Code::Limit);
                bytes += std::get<std::string>(value.data).size();
                if (bytes > max_output) return fail(Code::Limit);
                if (!valid_utf8(std::get<std::string>(value.data))) return fail(Code::Encoding);
                break;
            case Type::Enumeration:
                if (!std::holds_alternative<Enum>(value.data)) return fail(Code::Type);
                if (std::get<Enum>(value.data).index >= schema.choices.size()) return fail(Code::Range);
                break;
            case Type::Handle:
                if (!std::holds_alternative<Handle>(value.data)) return fail(Code::Type);
                if (!context.owns(std::get<Handle>(value.data), schema.domain)) return fail(Code::Unauthorized);
                break;
            case Type::Record: case Type::List: {
                if (!std::holds_alternative<Value::Children>(value.data)) return fail(Code::Type);
                const auto& children = std::get<Value::Children>(value.data);
                if ((schema.type == Type::Record && children.size() != schema.children.size()) ||
                    (schema.type == Type::List && children.size() > schema.count)) return fail(Code::Arguments);
                for (std::size_t index = 0; index < children.size(); ++index) {
                    auto checked = value_valid(children[index], schema.children[schema.type == Type::List ? 0 : index],
                                               context, depth + 1, nodes, bytes);
                    if (!checked) return checked;
                }
                break;
            }
            default: return fail(Code::Unsupported);
            }
            return {};
        }
    }
    bool terminal(Stage stage) noexcept { return stage != Stage::Queued && stage != Stage::Pending; }
    bool valid_utf8(std::string_view text) noexcept {
        for (std::size_t index = 0; index < text.size();) {
            const auto first = static_cast<unsigned char>(text[index++]);
            if (first < 0x80) {
                if ((first < 0x20 && first != '\t' && first != '\n' && first != '\r') || first == 0x7f) return false;
                continue;
            }
            std::size_t remaining;
            std::uint32_t code, minimum;
            if (first >= 0xc2 && first <= 0xdf) { remaining = 1; code = first & 0x1f; minimum = 0x80; }
            else if (first >= 0xe0 && first <= 0xef) { remaining = 2; code = first & 0x0f; minimum = 0x800; }
            else if (first >= 0xf0 && first <= 0xf4) { remaining = 3; code = first & 7; minimum = 0x10000; }
            else return false;
            if (remaining > text.size() - index) return false;
            while (remaining--) {
                const auto byte = static_cast<unsigned char>(text[index++]);
                if ((byte & 0xc0) != 0x80) return false;
                code = (code << 6) | (byte & 0x3f);
            }
            if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff) ||
                (code >= 0x80 && code <= 0x9f)) return false;
        }
        return true;
    }
    Checked<void> validate_schema(const Schema& schema) {
        std::size_t nodes = 0;
        return schema_valid(schema, 0, nodes);
    }
    Checked<void> validate(const Value& value, const Schema& schema, const Context& context) {
        auto checked = validate_schema(schema);
        if (!checked) return checked;
        std::size_t nodes = 0, bytes = 0;
        return value_valid(value, schema, context, 0, nodes, bytes);
    }
    bool Context::owns(const Handle& handle, std::string_view domain) const noexcept {
        return scope_ && scope_ == handle.scope_ && handle.key_ && handle.domain_ == domain;
    }
    Checked<Handle> Context::resolve(std::string_view name, std::string_view domain) const {
        if (!live()) return std::unexpected(Failure{Code::Expired});
        if (aliases_) for (const auto& alias : *aliases_)
            if (alias.name == name && owns(alias.handle, domain)) return alias.handle;
        return std::unexpected(Failure{Code::Unauthorized});
    }
    std::vector<std::string> Context::aliases(std::string_view domain) const {
        std::vector<std::string> result;
        if (live() && aliases_) for (const auto& alias : *aliases_)
            if (owns(alias.handle, domain)) result.push_back(alias.name);
        std::ranges::sort(result);
        return result;
    }
    Context Authority::issue(Rights rights) const {
        Context context;
        context.scope_ = scope_;
        context.rights_ = rights;
        context.aliases_ = std::make_shared<std::vector<Context::Alias>>();
        return context;
    }
    Checked<Handle> Authority::mint(std::string domain) {
        if (!scope_->active) return std::unexpected(Failure{Code::Expired});
        if (!name_valid(domain)) return std::unexpected(Failure{Code::Schema});
        if (next_ == std::numeric_limits<std::uint64_t>::max()) return std::unexpected(Failure{Code::Capacity});
        Handle handle;
        handle.scope_ = scope_;
        handle.domain_ = std::move(domain);
        handle.key_ = ++next_;
        return handle;
    }
    Checked<void> Authority::alias(Context& context, std::string name, Handle handle) const {
        if (!scope_->active || context.scope_ != scope_ || handle.scope_ != scope_) return fail(Code::Unauthorized);
        if (!name_valid(name)) return fail(Code::Schema);
        if (!context.aliases_ || context.aliases_->size() == max_commands) return fail(Code::Capacity);
        for (const auto& alias : *context.aliases_) if (alias.name == name) return fail(Code::Duplicate);
        context.aliases_->push_back({std::move(name), std::move(handle)});
        return {};
    }
    const Descriptor* ResolvedCommand::descriptor() const noexcept { return entry_ ? &entry_->descriptor : nullptr; }
    const Descriptor& BoundRequest::descriptor() const noexcept { return entry_->descriptor; }
    bool BoundRequest::same_body(const BoundRequest& other) const noexcept {
        return entry_ == other.entry_ && context_.same_scope(other.context_) && arguments_ == other.arguments_;
    }
    Registration& Registration::operator=(Registration&& other) noexcept {
        if (this != &other) { reset(); entry_ = std::move(other.entry_); }
        return *this;
    }
    Registration::~Registration() { reset(); }
    void Registration::reset() noexcept { if (entry_) entry_->active = false; entry_.reset(); }
    Registry::Registry(std::function<bool(const Context&)> accepts)
        : state_(std::make_shared<Detail::RegistryState>()), accepts_(std::move(accepts)) { state_->entries.reserve(max_commands); }
    Registry::~Registry() { for (auto& weak : state_->entries) if (auto entry = weak.lock()) entry->active = false; }
    Checked<Registration> Registry::install(Descriptor descriptor, std::span<const Type> signature, Handler handler) {
        try {
            if ((descriptor.kind != Kind::Query && descriptor.kind != Kind::Setting && descriptor.kind != Kind::Action) ||
                (descriptor.timing != Timing::OwnerImmediate && descriptor.timing != Timing::OwnerProgress) ||
                !name_valid(descriptor.name) || !name_valid(descriptor.domain) ||
                descriptor.help.size() > max_text || !valid_utf8(descriptor.help) ||
                descriptor.arguments.size() > max_arguments || signature.size() != descriptor.arguments.size() || !handler)
                return std::unexpected(Failure{Code::Schema});
            auto checked = validate_schema(descriptor.result);
            if (!checked) return std::unexpected(checked.error());
            for (std::size_t index = 0; index < signature.size(); ++index) {
                const auto& argument = descriptor.arguments[index];
                if (argument.schema.type != signature[index] || !name_valid(argument.name))
                    return std::unexpected(Failure{Code::Schema});
                checked = validate_schema(argument.schema);
                if (!checked) return std::unexpected(checked.error());
                for (std::size_t previous = 0; previous < index; ++previous)
                    if (argument.name == descriptor.arguments[previous].name) return std::unexpected(Failure{Code::Schema});
            }
            std::erase_if(state_->entries, [](const auto& weak) { auto entry = weak.lock(); return !entry || !entry->active; });
            for (const auto& weak : state_->entries)
                if (weak.lock()->descriptor.name == descriptor.name) return std::unexpected(Failure{Code::Duplicate});
            if (state_->entries.size() == max_commands) return std::unexpected(Failure{Code::Capacity});
            auto entry = std::make_shared<Detail::Entry>(Detail::Entry{std::move(descriptor), std::move(handler)});
            state_->entries.push_back(entry);
            return Registration(std::move(entry));
        } catch (const std::bad_alloc&) { return std::unexpected(Failure{Code::Allocation}); }
    }
    bool Registry::registered(std::string_view name) const noexcept {
        for (const auto& weak : state_->entries)
            if (auto entry = weak.lock(); entry && entry->active && entry->descriptor.name == name)
                return true;
        return false;
    }
    Checked<Descriptor> Registry::describe(std::string_view name, const Context& context) const {
        if (!context.live()) return std::unexpected(Failure{Code::Expired});
        if (accepts_ && !accepts_(context)) return std::unexpected(Failure{Code::Unauthorized});
        for (const auto& weak : state_->entries) if (auto entry = weak.lock())
            if (entry->active && context.permits(entry->descriptor.rights) && entry->descriptor.name == name)
                return entry->descriptor;
        return std::unexpected(Failure{Code::UnknownCommand});
    }
    std::vector<Descriptor> Registry::commands(const Context& context) const {
        std::vector<Descriptor> result;
        if (!context.live() || (accepts_ && !accepts_(context))) return result;
        for (const auto& weak : state_->entries) if (auto entry = weak.lock())
            if (entry->active && context.permits(entry->descriptor.rights)) result.push_back(entry->descriptor);
        std::ranges::sort(result, {}, &Descriptor::name);
        return result;
    }
    Checked<BoundRequest> Registry::bind(std::string_view name, std::vector<Value> arguments, const Context& context) const {
        auto resolved = resolve(name, context);
        if (!resolved) return std::unexpected(resolved.error());
        return bind(*resolved, std::move(arguments));
    }
    Checked<ResolvedCommand> Registry::resolve(std::string_view name, const Context& context) const {
        try {
            if (!context.live()) return std::unexpected(Failure{Code::Expired});
            if (accepts_ && !accepts_(context)) return std::unexpected(Failure{Code::Unauthorized});
            for (const auto& weak : state_->entries) if (auto entry = weak.lock()) {
                if (!entry->active || !context.permits(entry->descriptor.rights) || entry->descriptor.name != name) continue;
                ResolvedCommand resolved;
                resolved.entry_ = std::move(entry);
                resolved.registry_ = state_;
                resolved.context_ = context;
                return resolved;
            }
            return std::unexpected(Failure{Code::UnknownCommand});
        } catch (const std::bad_alloc&) { return std::unexpected(Failure{Code::Allocation}); }
    }
    Checked<BoundRequest> Registry::bind(const ResolvedCommand& resolved, std::vector<Value> arguments) const {
        try {
            if (!resolved.entry_) return std::unexpected(Failure{Code::Unregistered});
            const auto owner = resolved.registry_.lock();
            if (!owner) return std::unexpected(Failure{Code::Unregistered});
            if (owner != state_) return std::unexpected(Failure{Code::Unauthorized});
            const auto& context = resolved.context_;
            if (!context.live()) return std::unexpected(Failure{Code::Expired});
            if (accepts_ && !accepts_(context)) return std::unexpected(Failure{Code::Unauthorized});
            if (!resolved.entry_->active) return std::unexpected(Failure{Code::Unregistered});
            if (!context.permits(resolved.entry_->descriptor.rights)) return std::unexpected(Failure{Code::Unauthorized});
            if (arguments.size() != resolved.entry_->descriptor.arguments.size()) return std::unexpected(Failure{Code::Arguments});
            for (std::size_t index = 0; index < arguments.size(); ++index) {
                auto checked = validate(arguments[index], resolved.entry_->descriptor.arguments[index].schema, context);
                if (!checked) return std::unexpected(checked.error());
            }
            BoundRequest request;
            request.entry_ = resolved.entry_;
            request.context_ = context;
            request.arguments_ = std::move(arguments);
            return request;
        } catch (const std::bad_alloc&) { return std::unexpected(Failure{Code::Allocation}); }
    }
}
