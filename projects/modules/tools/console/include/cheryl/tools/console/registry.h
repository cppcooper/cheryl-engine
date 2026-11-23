#pragma once

#include <cheryl/tools/console/types.h>

#include <functional>
#include <array>
#include <span>
#include <tuple>
#include <utility>

namespace CE::Tools::Console {
    namespace Detail { struct Entry; struct RegistryState; }
    class Session;
    class CE_TYPE_API ResolvedCommand {
        std::shared_ptr<const Detail::Entry> entry_;
        std::weak_ptr<Detail::RegistryState> registry_;
        Context context_;
        friend class Registry;
    public:
        CE_CONSOLE_API const Descriptor* descriptor() const noexcept;
    };
    class CE_TYPE_API BoundRequest {
        std::shared_ptr<const Detail::Entry> entry_;
        Context context_;
        std::vector<Value> arguments_;
        friend class Registry;
        friend class Session;
    public:
        CE_CONSOLE_API const Descriptor& descriptor() const noexcept;
        std::span<const Value> arguments() const noexcept { return arguments_; }
        CE_CONSOLE_API bool same_body(const BoundRequest&) const noexcept;
    };
    class CE_TYPE_API Registration {
        std::shared_ptr<Detail::Entry> entry_;
        friend class Registry;
        explicit Registration(std::shared_ptr<Detail::Entry> entry) : entry_(std::move(entry)) {}
    public:
        Registration() = default;
        Registration(const Registration&) = delete;
        Registration& operator=(const Registration&) = delete;
        Registration(Registration&&) noexcept = default;
        CE_CONSOLE_API Registration& operator=(Registration&&) noexcept;
        CE_CONSOLE_API ~Registration();
        CE_CONSOLE_API void reset() noexcept;
    };
    template<class ValueType> inline constexpr Type value_type = Type::Record;
    template<> inline constexpr Type value_type<bool> = Type::Boolean;
    template<> inline constexpr Type value_type<std::uint64_t> = Type::Unsigned;
    template<> inline constexpr Type value_type<std::int64_t> = Type::Integer;
    template<> inline constexpr Type value_type<double> = Type::Scalar;
    template<> inline constexpr Type value_type<std::string> = Type::Text;
    template<> inline constexpr Type value_type<Enum> = Type::Enumeration;
    template<> inline constexpr Type value_type<Handle> = Type::Handle;
    // Registry, registrations, contexts, and sessions share one serialized owner.
    class CE_TYPE_API Registry {
        std::shared_ptr<Detail::RegistryState> state_;
        std::function<bool(const Context&)> accepts_;
        using Handler = std::function<std::shared_ptr<Work>(const Context&, std::span<const Value>)>;
        CE_CONSOLE_API Checked<Registration> install(Descriptor, std::span<const Type>, Handler);
    public:
        // A pure, non-reentrant owner gate rejects unrelated issuers before discovery.
        CE_CONSOLE_API explicit Registry(std::function<bool(const Context&)> accepts = {});
        Registry(const Registry&) = delete;
        Registry& operator=(const Registry&) = delete;
        CE_CONSOLE_API ~Registry();
        // Callbacks only prepare Work. Effects belong in step(), after retention.
        template<class... Args, class Callback>
        Checked<Registration> add(Descriptor descriptor, Callback callback) {
            static_assert(((value_type<Args> != Type::Record) && ...), "Unsupported command argument type");
            const std::array<Type, sizeof...(Args)> signature{value_type<Args>...};
            return install(std::move(descriptor), signature,
                [callback = std::move(callback)](const Context& context, std::span<const Value> arguments) {
                    return [&]<std::size_t... Index>(std::index_sequence<Index...>) {
                        return std::invoke(callback, context, std::get<Args>(arguments[Index].data)...);
                    }(std::index_sequence_for<Args...>{});
                });
        }
        CE_CONSOLE_API Checked<Descriptor> describe(std::string_view name, const Context&) const;
        CE_CONSOLE_API std::vector<Descriptor> commands(const Context&) const;
        CE_CONSOLE_API Checked<ResolvedCommand> resolve(std::string_view name, const Context&) const;
        CE_CONSOLE_API Checked<BoundRequest> bind(const ResolvedCommand&, std::vector<Value> arguments) const;
        CE_CONSOLE_API Checked<BoundRequest> bind(std::string_view name, std::vector<Value> arguments, const Context&) const;
    };
}
