#include <magic/runtime.h>

#include <algorithm>
#include <limits>

namespace Magic::Contact {
    bool supports(const PlanHandle& plan) noexcept {
        if (!plan)
            return false;
        const auto& m = plan->meaning();
        if (m.entry != Entry{} || m.delivery != Delivery::Contact || m.onset_ticks != 0 ||
            m.bindings.size() != 1 || m.effects.size() != 1)
            return false;
        const auto& b = m.bindings[0];
        const auto& e = m.effects[0];
        return b.query == Query::WillingTouchedActor && b.query_time == BindingTime::Release &&
               b.anchor_time == BindingTime::Release && b.radius_mm == 0 && b.inner_radius_mm == 0 &&
               e.primitive == Primitive::Heal && e.binding == 0 && e.quantity > 0 && e.duration_ticks == 0 &&
               std::holds_alternative<std::monostate>(e.payload);
    }
    std::uint32_t healed(std::uint32_t health, std::uint32_t cap, std::uint32_t quantity) noexcept {
        return static_cast<std::uint32_t>(std::min<std::uint64_t>(cap, static_cast<std::uint64_t>(health) + quantity));
    }
    std::expected<Evaluation, Error> evaluate(const PlanHandle& plan, Request request, const WorldAdapter& world, Limits limits) {
        if (!plan)
            return std::unexpected(Error::NoPlan);
        if (plan->metrics().retained_bytes > limits.plan_bytes)
            return std::unexpected(Error::PlanLimit);
        if (!supports(plan))
            return std::unexpected(Error::Unsupported);
        if (limits.result_bytes < sizeof(Change))
            return std::unexpected(Error::ResultLimit);
        const auto& catalog = world.catalog();
        if (catalog.operations.size() > primitive_count || !catalog.spoken || !catalog.contact)
            return std::unexpected(Error::Schema);
        // Exact bounded units: fixed interpretation/facts/candidate overhead plus
        // binding text, effect key and every visited schema key (including terminators).
        const auto& b = plan->meaning().bindings[0];
        const auto& e = plan->meaning().effects[0];
        std::size_t work = 16;
        auto spend = [&](std::size_t n) {
            if (work > limits.work || n > limits.work - work)
                return false;
            work += n;
            return true;
        };
        if (!spend(b.name.size()) || !spend(b.frame.size()) || !spend(e.key.size() + 1))
            return std::unexpected(Error::WorkLimit);
        bool supported = false;
        for (const auto& schema : catalog.operations) {
            if (schema.key.size() > 64)
                return std::unexpected(Error::Schema);
            if (!spend(schema.key.size() + 1))
                return std::unexpected(Error::WorkLimit);
            if (schema.primitive == e.primitive && schema.key == e.key) {
                if (supported || e.quantity > schema.max_quantity)
                    return std::unexpected(Error::Schema);
                supported = true;
            }
        }
        if (!supported)
            return std::unexpected(Error::Schema);
        auto facts = world.inspect(request);
        if (!facts)
            return std::unexpected(facts.error());
        if (!facts->caster_living || !facts->can_cast)
            return std::unexpected(Error::Permission);
        if (!facts->recipient_living || !facts->eligible || !facts->willing || !facts->touching)
            return std::unexpected(Error::Contact);
        if (facts->health_cap == 0 || facts->health > facts->health_cap)
            return std::unexpected(Error::Schema);
        if (facts->resource < facts->price)
            return std::unexpected(Error::Resources);
        return Evaluation{{request, *facts, healed(facts->health, facts->health_cap, e.quantity), facts->resource - facts->price}, work};
    }
    Runtime::Runtime(WorldAdapter& world, Limits limits)
    : world_(world), limits_(limits), incarnation_(std::make_shared<const unsigned char>(0)) {}
    std::expected<void, Error> Runtime::authorize(const Ticket& ticket) const noexcept {
        if (!ticket.incarnation_)
            return std::unexpected(Error::InvalidTicket);
        if (ticket.incarnation_ != incarnation_)
            return std::unexpected(Error::ForeignTicket);
        if (!prepared_)
            return std::unexpected(Error::ConsumedTicket);
        if (prepared_->serial != ticket.serial_)
            return std::unexpected(Error::StaleTicket);
        return {};
    }
    std::expected<Ticket, Error> Runtime::prepare(const PlanHandle& plan, Request request) {
        auto value = evaluate(plan, request, world_, limits_);
        if (!value)
            return std::unexpected(value.error());
        if (issued_ >= limits_.tickets || issued_ == std::numeric_limits<std::uint64_t>::max())
            return std::unexpected(Error::TicketLimit);
        Ticket ticket;
        ticket.incarnation_ = incarnation_;
        ticket.serial_ = issued_ + 1;
        prepared_ = Prepared{plan, request, value->change.before, ticket.serial_};
        issued_ = ticket.serial_;
        return ticket;
    }
    std::expected<Result, Error> Runtime::release(const Ticket& ticket) {
        auto authorized = authorize(ticket);
        if (!authorized)
            return std::unexpected(authorized.error());
        auto value = evaluate(prepared_->plan, prepared_->request, world_, limits_);
        if (!value)
            return std::unexpected(value.error());
        if (value->change.before != prepared_->facts)
            return std::unexpected(Error::StaleTicket);
        auto candidate = world_.candidate(value->change);
        if (!candidate)
            return std::unexpected(candidate.error());
        if (!*candidate)
            return std::unexpected(Error::Preparation);
        // All throwing storage allocation precedes adoption; reports own scalar values.
        Result result = std::make_shared<const Change>(value->change);
        auto adopted = (*candidate)->adopt();
        if (!adopted)
            return std::unexpected(adopted.error());
        prepared_.reset();
        return result;
    }
    std::expected<void, Error> Runtime::discard(const Ticket& ticket) {
        auto authorized = authorize(ticket);
        if (!authorized)
            return authorized;
        prepared_.reset();
        return {};
    }
}
