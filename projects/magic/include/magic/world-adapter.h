#pragma once

#include <magic/compiler.h>
#include <expected>
#include <memory>

namespace Magic::Contact {
    enum class Error {
        NoPlan, Unsupported, Schema, MissingEntity, Permission, Contact, Resources,
        PlanLimit, WorkLimit, ResultLimit, TicketLimit, InvalidTicket, ForeignTicket,
        StaleTicket, ConsumedTicket, Preparation, RevisionLimit
    };
    // Portable logical identity, interpreted only by the supplying owner; never a storage index.
    struct Entity {
        std::uint64_t domain = 0, key = 0, generation = 0;
        bool operator==(const Entity&) const = default;
    };
    struct Request {
        Entity caster, recipient;
        bool operator==(const Request&) const = default;
    };
    struct Facts {
        std::uint64_t revision = 0, schema_revision = 0;
        std::uint32_t health = 0, health_cap = 0, resource = 0, price = 0;
        bool caster_living = false, can_cast = false, recipient_living = false;
        bool eligible = false, willing = false, touching = false;
        bool operator==(const Facts&) const = default;
    };
    struct Change {
        Request request;
        Facts before;
        std::uint32_t health = 0, resource = 0;
        bool operator==(const Change&) const = default;
    };
    // A candidate owns both debit and Heal. Destruction is non-effectful. adopt must
    // recheck owner revision/incarnation and adopt the whole aggregate or do nothing.
    class Candidate {
    public:
        virtual ~Candidate() = default;
        [[nodiscard]] virtual std::expected<void, Error> adopt() noexcept = 0;
    };
    // Borrowed by one serialized simulation owner. The adapter and aggregate must
    // outlive their runtime. No ticket owns, revives or calls a destroyed adapter.
    // inspect/candidate do not mutate the world; candidate failures and exceptions
    // preserve it. Every owner edit, including policy/schema replacement, advances
    // revision (no wrap/reuse). Removed/reused logical names advance generation.
    // Candidate adoption rechecks that identity, revision and schema together.
    class WorldAdapter {
    public:
        virtual ~WorldAdapter() = default;
        [[nodiscard]] virtual const Catalog& catalog() const noexcept = 0;
        [[nodiscard]] virtual std::expected<Facts, Error> inspect(Request request) const = 0;
        [[nodiscard]] virtual std::expected<std::unique_ptr<Candidate>, Error> candidate(const Change& change) = 0;
    };
}
