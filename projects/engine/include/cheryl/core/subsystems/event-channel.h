#pragma once

#include <concepts>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

namespace CE::Events {
/** Payload must be an unqualified copy-constructible object type; references and cv-qualified payload declarations are excluded.
     * @tparam T Exact in-process payload type.
     */
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
        /** Callback borrows one invocation-owned payload copy; copy it to retain beyond the call. */
        using Callback = std::function<void(const Payload&)>;

        explicit EventChannel(std::string name)
        : name_(std::move(name)) {}

        [[nodiscard]] const std::string& name() const noexcept { return name_; }
    };
}

/**
 * @fn template<CE::Events::EventPayload Payload> CE::Events::EventChannel<Payload>::EventChannel(std::string name)
 * @brief Own the exact channel name for this payload type.
 * @param name Copied/moved name; empty is permitted and not normalized.
 * Equal names with different payload types and legacy named channels remain separate. Type identity is process-local, not a serialization format.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn template<CE::Events::EventPayload Payload> const std::string& CE::Events::EventChannel<Payload>::name() const noexcept
 * @brief Borrow the retained exact channel name until this value is changed or destroyed.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */
