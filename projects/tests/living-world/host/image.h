#pragma once

#include <ports/values.h>
#include <magic/runtime.h>

#include <array>
#include <optional>
#include <type_traits>

namespace CE::LivingWorld {
    enum class Error {
        Unauthorized, WrongScope, StaleFence, Conflict, Capacity, InvalidBody,
        Stale, Exhausted, Busy, Missing, Navigation, Preparation
    };
    enum class Disposition { Queued, Applied, Refused };
    enum class Reason { None, Stale, Permission, Contact, Resources, Schema, Preparation };
    struct Quote {
        std::uint64_t id = 1, revision = 1, resource = 1;
        std::uint32_t price = 3;
        bool operator==(const Quote&) const = default;
    };
    struct PlanBinding {
        std::uint64_t id = 1, revision = 1, catalog = 1, schema = 1;
        std::array<char, 8192> source{};
        std::size_t source_size = 0;
        std::array<char, 64> operation{};
        std::size_t operation_size = 0;
        std::uint32_t max_quantity = 0, max_duration = 0, max_coordinate = 0;
        std::uint16_t capabilities = 0;
        std::uint8_t appearances = 0;
        bool operator==(const PlanBinding&) const = default;
    };
    struct HealBody {
        PlanBinding plan;
        EntityRef caster, recipient;
        Quote quote;
        ReadSet reads;
        std::uint64_t intent = 1;
        bool operator==(const HealBody&) const = default;
    };
    struct Action {
        AttemptKey key;
        HealBody body;
        bool operator==(const Action&) const = default;
    };
    struct Receipt {
        Disposition state = Disposition::Queued;
        Reason reason = Reason::None;
        std::uint32_t gain = 0;
        bool operator==(const Receipt&) const = default;
    };
    struct Actor {
        EntityRef ref;
        std::uint64_t revision = 1;
        std::uint32_t health = 12, cap = 20, reserve = 10;
        bool living = true, can_cast = true, eligible = true;
        bool willing = true, touching = true, observes = false, access = true;
        bool operator==(const Actor&) const = default;
    };
    namespace Detail {
        struct Record {
            Action action;
            OwnerFence fence;
            Receipt receipt;
            bool cancelled = false;
            std::array<EntityRef, 4> witnesses{};
            std::size_t witness_count = 0;
            std::uint64_t observed_at = 0;
            std::optional<::Magic::Contact::Change> applied{};
        };
        struct AcceptedPlan {
            PlanBinding binding;
            ::Magic::PlanHandle plan;
        };
        struct ObservationWork {
            AttemptKey cause;
            EntityRef witness, subject;
            std::uint64_t observed_at = 0;
            bool consumed = false;
        };
        struct Acquired {
            AttemptKey cause;
            EntityRef witness, subject;
            std::uint64_t local_id = 0, observed_at = 0, acquired_at = 0;
        };
        struct Image {
            WorldScope scope;
            OwnerFence fence;
            std::uint64_t revision = 1, policy_revision = 1, tick = 0;
            Quote quote;
            PlanBinding binding;
            std::array<AcceptedPlan, 8> plans{};
            std::size_t plan_count = 0;
            std::array<Actor, 8> actors{};
            std::size_t actor_count = 0;
            std::array<Record, 16> records{};
            std::size_t record_count = 0;
            std::array<ObservationWork, 16> work{};
            std::size_t work_count = 0;
            std::array<Acquired, 32> acquired{};
            std::size_t acquired_count = 0;
        };
        static_assert(std::is_nothrow_copy_assignable_v<Image>);
        static_assert(std::is_nothrow_move_assignable_v<Image>);
        static_assert(std::is_nothrow_copy_constructible_v<Record>);
        static_assert(std::is_nothrow_copy_assignable_v<Receipt>);
    }
}
