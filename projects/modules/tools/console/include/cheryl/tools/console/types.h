#pragma once

#include <cheryl/export.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace CE::Tools::Console {
    inline constexpr std::size_t max_commands = 32, max_arguments = 8, max_requests = 32;
    inline constexpr std::size_t max_line = 4096, max_text = 1024, max_tokens = 16;
    inline constexpr std::size_t max_output = 8192;
    inline constexpr std::size_t max_nodes = 64, max_depth = 4;
    enum class Code {
        None, Unauthorized, Expired, UnknownCommand, Duplicate, Schema, Unsupported,
        Syntax, Encoding, Limit, Type, Range, Arguments, Conflict, Unregistered,
        Capacity, Allocation, Owner, Cancelled, Reentrant
    };
    struct Failure {
        Code code = Code::None;
        std::size_t offset = 0, length = 0;
        bool operator==(const Failure&) const = default;
    };
    template<class Type> using Checked = std::expected<Type, Failure>;
    enum class Type { Boolean, Unsigned, Integer, Scalar, Text, Enumeration, Handle, Record, List };
    enum class Kind { Query, Setting, Action };
    enum class Timing { OwnerImmediate, OwnerProgress };
    enum class Stage { Queued, Pending, Succeeded, Refused, Failed, Cancelled };
    using Rights = std::uint32_t;
    struct Scope { bool active = true; };
    class Authority;
    class Context;
    class Handle {
        std::shared_ptr<const Scope> scope_;
        std::string domain_;
        std::uint64_t key_ = 0;
        friend class Authority;
        friend class Context;
    public:
        Handle() = default;
        bool operator==(const Handle&) const = default;
        std::uint64_t key() const noexcept { return key_; }
        std::string_view domain() const noexcept { return domain_; }
    };
    struct Enum {
        std::uint32_t index = 0;
        bool operator==(const Enum&) const = default;
    };
    struct Value {
        using Children = std::vector<Value>;
        Type type = Type::Text;
        std::variant<bool, std::uint64_t, std::int64_t, double, std::string, Enum, Handle, Children> data = std::string{};
        static Value boolean(bool value) { return {Type::Boolean, value}; }
        static Value unsigned_integer(std::uint64_t value) { return {Type::Unsigned, value}; }
        static Value integer(std::int64_t value) { return {Type::Integer, value}; }
        static Value scalar(double value) { return {Type::Scalar, value}; }
        static Value text(std::string value) { return {Type::Text, std::move(value)}; }
        static Value enumeration(std::uint32_t value) { return {Type::Enumeration, Enum{value}}; }
        static Value handle(Handle value) { return {Type::Handle, std::move(value)}; }
        static Value record(Children value) { return {Type::Record, std::move(value)}; }
        static Value list(Children value) { return {Type::List, std::move(value)}; }
        bool operator==(const Value&) const = default;
    };
    struct Schema {
        Type type = Type::Text;
        std::uint64_t unsigned_max = std::numeric_limits<std::uint64_t>::max();
        std::int64_t integer_min = std::numeric_limits<std::int64_t>::min();
        std::int64_t integer_max = std::numeric_limits<std::int64_t>::max();
        std::string unit, domain;
        std::vector<std::string> choices, fields;
        std::vector<Schema> children;
        std::size_t count = max_arguments;
        std::size_t text_limit = max_text;
        bool operator==(const Schema&) const = default;
    };
    struct Argument {
        std::string name;
        Schema schema;
        bool operator==(const Argument&) const = default;
    };
    struct Descriptor {
        std::string name, help, domain;
        Kind kind = Kind::Query;
        Timing timing = Timing::OwnerImmediate;
        Rights rights = 0;
        std::vector<Argument> arguments;
        Schema result;
        bool operator==(const Descriptor&) const = default;
    };
    class CE_TYPE_API Context {
        struct Alias { std::string name; Handle handle; };
        std::shared_ptr<const Scope> scope_;
        Rights rights_ = 0;
        std::shared_ptr<std::vector<Alias>> aliases_;
        friend class Authority;
    public:
        Context() = default;
        bool live() const noexcept { return scope_ && scope_->active; }
        bool permits(Rights rights) const noexcept { return live() && (rights_ & rights) == rights; }
        CE_CONSOLE_API bool owns(const Handle& handle, std::string_view domain) const noexcept;
        CE_CONSOLE_API Checked<Handle> resolve(std::string_view name, std::string_view domain) const;
        CE_CONSOLE_API std::vector<std::string> aliases(std::string_view domain) const;
        bool same_scope(const Context& other) const noexcept { return scope_ == other.scope_ && rights_ == other.rights_; }
    };
    class CE_TYPE_API Authority {
        std::shared_ptr<Scope> scope_ = std::make_shared<Scope>();
        std::uint64_t next_ = 0;
    public:
        Authority() = default;
        Authority(const Authority&) = delete;
        Authority& operator=(const Authority&) = delete;
        ~Authority() { expire(); }
        CE_CONSOLE_API Context issue(Rights rights) const;
        CE_CONSOLE_API Checked<Handle> mint(std::string domain);
        CE_CONSOLE_API Checked<void> alias(Context&, std::string name, Handle) const;
        void expire() noexcept { scope_->active = false; }
    };
    struct Result {
        Stage stage = Stage::Queued;
        Failure failure;
        Value value;
    };
    class CE_CONSOLE_API Work {
    public:
        Result result;
        virtual ~Work();
        virtual void step() noexcept = 0;
        virtual void close() noexcept = 0;
    };
    CE_CONSOLE_API bool terminal(Stage stage) noexcept;
    CE_CONSOLE_API bool valid_utf8(std::string_view text) noexcept;
    CE_CONSOLE_API Checked<void> validate_schema(const Schema& schema);
    CE_CONSOLE_API Checked<void> validate(const Value&, const Schema&, const Context&);
}
