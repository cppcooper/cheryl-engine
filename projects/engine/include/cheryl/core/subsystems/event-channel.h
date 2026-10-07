#pragma once

#include <concepts>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

namespace CE::SubSystems {
    template <typename T>
    concept EventPayload = std::is_object_v<T> && std::same_as<T, std::remove_cvref_t<T>> && std::is_copy_constructible_v<T>;

    /** Owned exact name and payload type identify one typed channel within a bus.
     * Same name/type values are equivalent; other payload types and legacy named
     * channels are separate. Type identity is in-process, not a serialized protocol.
     * Callback arguments borrow an invocation-owned copy; retain a copy for later use.
     */
    template <EventPayload Payload> class EventChannel final {
        std::string name_;

    public:
        using Callback = std::function<void(const Payload&)>;

        explicit EventChannel(std::string name)
        : name_(std::move(name)) {}

        [[nodiscard]] const std::string& name() const noexcept { return name_; }
    };
}
