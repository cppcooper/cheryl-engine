#include <magic/reference-host.h>

#include <algorithm>
#include <limits>
#include <type_traits>
#include <utility>

namespace Magic::Reference {
    namespace {
        [[noreturn]] void refuse(Error code) {
            throw Failure{code};
        }
        bool point_valid(Point point) {
            return point.x >= -1'000'000 && point.x <= 1'000'000 && point.y >= -1'000'000 && point.y <= 1'000'000;
        }
        bool spec_valid(const ActorSpec& actor) {
            return point_valid(actor.position) && actor.max_health > 0 && actor.max_health <= 1'000'000 &&
                   actor.health <= actor.max_health && actor.mana <= 1'000'000 && actor.base_speed_mm_per_s <= 1'000'000;
        }
        bool catalog_valid(const Catalog& catalog) {
            if (catalog.operations.size() > 3)
                return false;
            for (std::size_t i = 0; i < catalog.operations.size(); ++i) {
                const auto& schema = catalog.operations[i];
                if (schema.key.empty() || schema.key.size() > 64 || schema.max_quantity == 0 || schema.max_quantity > 1'000'000 ||
                    schema.max_duration_ticks > 8'640'000 ||
                    (schema.primitive != Primitive::Heal && schema.primitive != Primitive::Fire && schema.primitive != Primitive::Slow))
                    return false;
                for (char c : schema.key)
                    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
                        return false;
                for (std::size_t j = 0; j < i; ++j)
                    if (schema.key == catalog.operations[j].key || schema.primitive == catalog.operations[j].primitive)
                        return false;
            }
            return true;
        }
        bool limits_valid(const HostLimits& l) {
            const HostLimits c;
            return l.actors <= c.actors && l.pending <= c.pending && l.statuses <= c.statuses && l.selected_targets <= c.selected_targets &&
                   l.selector_work <= c.selector_work && l.work <= c.work && l.plan_bytes <= c.plan_bytes && l.max_tick <= c.max_tick &&
                   l.max_action <= c.max_action && l.max_ticket <= c.max_ticket && l.max_revision <= c.max_revision &&
                   l.max_generation <= c.max_generation;
        }
        const ActorView& actor(const Snapshot& world, ActorId id) {
            if (id.slot >= actor_capacity || id.generation == 0 || !world.actors[id.slot].present || world.actors[id.slot].id != id)
                refuse(Error::MissingActor);
            return world.actors[id.slot];
        }
        void revision(Snapshot& world, const HostLimits& limits) {
            if (world.revision == limits.max_revision)
                refuse(Error::RevisionLimit);
            ++world.revision;
        }
        std::uint64_t later(std::uint64_t now, std::uint64_t duration, const HostLimits& limits) {
            if (duration > limits.max_tick - now)
                refuse(Error::TimeLimit);
            return now + duration;
        }
        std::uint64_t square(std::uint64_t value) {
            if (value != 0 && value > std::numeric_limits<std::uint64_t>::max() / value)
                refuse(Error::InvalidAim);
            return value * value;
        }
        bool within(Point a, Point b, std::uint32_t radius) {
            // Validated +/-1e6 positions keep subtraction in int64; multiply/add are checked independently.
            const auto dx = static_cast<std::int64_t>(a.x) - b.x;
            const auto dy = static_cast<std::int64_t>(a.y) - b.y;
            const auto x2 = square(static_cast<std::uint64_t>(dx < 0 ? -dx : dx));
            const auto y2 = square(static_cast<std::uint64_t>(dy < 0 ? -dy : dy));
            if (x2 > std::numeric_limits<std::uint64_t>::max() - y2)
                refuse(Error::InvalidAim);
            return x2 + y2 <= square(radius);
        }
    }

    struct ReferenceHost::Meter {
        const HostLimits& limits;
        Usage usage;
        std::uint64_t action = 0;
        std::uint64_t tick = 0;
        explicit Meter(const HostLimits& value)
        : limits(value) {
            spend(80);
        }
        [[noreturn]] void fail(Error code) const { throw Failure{code, action, tick}; }
        void spend(std::size_t count = 1) {
            if (count > limits.work - usage.work)
                fail(Error::WorkLimit);
            usage.work += count;
        }
        void visit() {
            spend();
            if (usage.selector_work == limits.selector_work)
                fail(Error::SelectorLimit);
            ++usage.selector_work;
        }
        void select() {
            spend();
            if (usage.selected_targets == limits.selected_targets)
                fail(Error::TargetLimit);
            ++usage.selected_targets;
        }
    };

    ReferenceHost::ReferenceHost(Catalog catalog, HostLimits limits)
    : catalog_(std::move(catalog)), limits_(limits), owner_(std::make_shared<const std::uint8_t>(0)) {
        for (std::size_t i = 0; i < actor_capacity; ++i)
            state_.world.actors[i].id.slot = static_cast<std::uint32_t>(i);
    }

    std::expected<std::unique_ptr<ReferenceHost>, Failure> ReferenceHost::create(const Catalog& catalog, HostLimits limits) {
        if (!limits_valid(limits))
            return std::unexpected(Failure{Error::InvalidLimits});
        if (!catalog_valid(catalog))
            return std::unexpected(Failure{Error::InvalidCatalog});
        return std::unique_ptr<ReferenceHost>(new ReferenceHost(catalog, limits));
    }
    std::expected<ActorId, Failure> ReferenceHost::add_actor(const ActorSpec& data) {
        try {
            Meter meter(limits_);
            if (!spec_valid(data))
                refuse(Error::InvalidActor);
            auto next = state_.world;
            for (std::size_t i = 0; i < limits_.actors; ++i) {
                meter.spend();
                auto& slot = next.actors[i];
                if (slot.present)
                    continue;
                if (slot.id.generation == limits_.max_generation)
                    refuse(Error::GenerationLimit);
                revision(next, limits_);
                ++slot.id.generation;
                slot.present = true;
                slot.data = data;
                slot.slow = {};
                state_.world = next;
                return slot.id;
            }
            refuse(Error::ActorLimit);
        } catch (Failure failure) {
            return std::unexpected(failure);
        }
    }
    std::expected<void, Failure> ReferenceHost::update_actor(ActorId id, const ActorSpec& data) {
        try {
            Meter meter(limits_);
            static_cast<void>(actor(state_.world, id));
            if (!spec_valid(data))
                refuse(Error::InvalidActor);
            auto next = state_.world;
            revision(next, limits_);
            next.actors[id.slot].data = data;
            state_.world = next;
            return {};
        } catch (Failure failure) {
            return std::unexpected(failure);
        }
    }
    std::expected<void, Failure> ReferenceHost::remove_actor(ActorId id) {
        try {
            Meter meter(limits_);
            static_cast<void>(actor(state_.world, id));
            auto next = state_.world;
            revision(next, limits_);
            next.actors[id.slot].present = false;
            next.actors[id.slot].data = {};
            next.actors[id.slot].slow = {};
            state_.world = next;
            return {};
        } catch (Failure failure) {
            return std::unexpected(failure);
        }
    }
    std::expected<void, Failure> ReferenceHost::replace_catalog(const Catalog& catalog) {
        try {
            Meter meter(limits_);
            if (!catalog_valid(catalog))
                refuse(Error::InvalidCatalog);
            auto next = state_.world;
            revision(next, limits_);
            Catalog owned = catalog; // bounded copy before any mutation
            static_assert(std::is_nothrow_swappable_v<Catalog>);
            std::swap(catalog_, owned);
            state_.world = next;
            return {};
        } catch (Failure failure) {
            return std::unexpected(failure);
        }
    }

    void ReferenceHost::validate(const PlanHandle& plan, const Request& request, const State& state, Meter& meter, bool paying) const {
        if (!plan)
            meter.fail(Error::NoPlan);
        if (plan->metrics().retained_bytes > limits_.plan_bytes)
            meter.fail(Error::PlanLimit);
        const auto& meaning = plan->meaning();
        if (meaning.entry.kind != EntryKind::Spoken || meaning.entry.mode != CastingMode::Release ||
            meaning.entry.trigger != Trigger::None || meaning.entry.charges != 0)
            meter.fail(Error::UnsupportedEntry);
        if (meaning.delivery != Delivery::Contact && meaning.delivery != Delivery::Burst)
            meter.fail(Error::UnsupportedDelivery);
        if (!catalog_.spoken || (meaning.delivery == Delivery::Contact && !catalog_.contact) ||
            (meaning.delivery == Delivery::Burst && !catalog_.burst) || (meaning.effects.size() > 1 && !catalog_.together))
            meter.fail(Error::SchemaMismatch);
        const auto& caster = actor(state.world, request.caster);
        if (!caster.data.can_cast || caster.data.health == 0)
            meter.fail(Error::Permission);
        if (meaning.delivery == Delivery::Contact) {
            if (!request.contact || request.aim || meaning.onset_ticks != 0)
                meter.fail(Error::Contact);
            const auto& recipient = actor(state.world, *request.contact);
            if (!recipient.data.eligible || !recipient.data.willing || recipient.data.health == 0 ||
                !within(caster.data.position, recipient.data.position, 50))
                meter.fail(Error::Contact);
        } else if (!request.aim || request.contact || !point_valid(*request.aim))
            meter.fail(Error::InvalidAim);
        for (const auto& binding : meaning.bindings) {
            meter.spend(binding.name.size() + binding.frame.size() + 1);
            if (meaning.delivery == Delivery::Contact) {
                if (binding.query != Query::WillingTouchedActor || binding.query_time != BindingTime::Release)
                    meter.fail(Error::UnsupportedBinding);
            } else if (
                binding.query != Query::EligibleDisk || binding.frame != "G0" || !binding.inclusive ||
                binding.anchor_time != BindingTime::Release || binding.query_time != BindingTime::Activation
            )
                meter.fail(Error::UnsupportedBinding);
        }
        for (const auto& effect : meaning.effects) {
            bool supported = false;
            for (const auto& schema : catalog_.operations) {
                meter.spend(effect.key.size() + 1);
                if (effect.key == schema.key && effect.primitive == schema.primitive && effect.quantity > 0 &&
                    effect.quantity <= schema.max_quantity && effect.duration_ticks <= schema.max_duration_ticks &&
                    ((effect.primitive == Primitive::Slow && effect.quantity <= 1000 && effect.duration_ticks > 0) ||
                     (effect.primitive != Primitive::Slow && effect.duration_ticks == 0)))
                    supported = true;
            }
            if (!supported)
                meter.fail(Error::SchemaMismatch);
        }
        if (paying)
            static_cast<void>(later(state.world.tick, meaning.onset_ticks, limits_));
        const auto cost = static_cast<std::uint32_t>(meaning.effects.size() * 10); // M1 has <=4 effects
        if (paying && caster.data.mana < cost)
            meter.fail(Error::Resources);
    }

    void ReferenceHost::apply(const PlanHandle& plan, const Request& request, State& state, Meter& meter) const {
        const auto& meaning = plan->meaning();
        std::array<std::array<bool, actor_capacity>, 8> selected{};
        // Resolve every binding against the same pre-activation world before building any writes.
        for (std::size_t b = 0; b < meaning.bindings.size(); ++b) {
            bool any = false;
            const auto& binding = meaning.bindings[b];
            if (binding.query == Query::WillingTouchedActor) {
                meter.visit();
                meter.select();
                selected[b][request.contact->slot] = true;
                any = true;
            } else {
                for (std::size_t i = 0; i < actor_capacity; ++i) {
                    meter.visit();
                    const auto& slot = state.world.actors[i];
                    if (slot.present && slot.data.health > 0 && slot.data.eligible &&
                        within(slot.data.position, *request.aim, binding.radius_mm)) {
                        meter.select();
                        selected[b][i] = true;
                        any = true;
                    }
                }
            }
            if (!any)
                meter.fail(Error::EmptySelection);
        }
        struct Writes {
            int health = 0;
            std::uint32_t quantity = 0;
            bool slow = false;
            std::uint32_t permille = 1000;
            std::uint64_t until = 0;
        };
        std::array<Writes, actor_capacity> writes{};
        for (const auto& effect : meaning.effects) {
            for (std::size_t i = 0; i < actor_capacity; ++i) {
                meter.spend();
                if (!selected[effect.binding][i])
                    continue;
                auto& write = writes[i];
                if (effect.primitive == Primitive::Slow) {
                    if (write.slow)
                        meter.fail(Error::Overlap);
                    write.slow = true;
                    write.permille = effect.quantity;
                    write.until = later(state.world.tick, effect.duration_ticks, limits_);
                } else {
                    if (write.health != 0)
                        meter.fail(Error::Overlap);
                    write.health = effect.primitive == Primitive::Heal ? 1 : -1;
                    write.quantity = effect.quantity;
                }
            }
        }
        std::size_t statuses = 0;
        for (std::size_t i = 0; i < actor_capacity; ++i) {
            meter.spend();
            const bool active = state.world.actors[i].present && state.world.actors[i].slow.until_tick != 0;
            if (active)
                ++statuses;
            if (writes[i].slow) {
                if (active)
                    meter.fail(Error::ActiveSlow);
                ++statuses;
            }
        }
        if (statuses > limits_.statuses)
            meter.fail(Error::StatusLimit);
        for (std::size_t i = 0; i < actor_capacity; ++i) {
            meter.spend();
            auto& data = state.world.actors[i];
            const auto& write = writes[i];
            if (write.health == 1)
                data.data.health = static_cast<std::uint32_t>(
                    std::min<std::uint64_t>(data.data.max_health, static_cast<std::uint64_t>(data.data.health) + write.quantity)
                );
            else if (write.health == -1)
                data.data.health = write.quantity >= data.data.health ? 0 : data.data.health - write.quantity;
            if (write.slow)
                data.slow = {write.permille, write.until};
        }
    }

    std::expected<Preview, Failure> ReferenceHost::preview(const PlanHandle& plan, const Request& request) const {
        try {
            Meter meter(limits_);
            validate(plan, request, state_, meter, true);
            const auto due = later(state_.world.tick, plan->meaning().onset_ticks, limits_);
            if (plan->meaning().onset_ticks == 0) {
                auto scratch = state_;
                apply(plan, request, scratch, meter);
            } // Delayed preview cannot promise future members/status capacity.
            return Preview{static_cast<std::uint32_t>(plan->meaning().effects.size() * 10), due, meter.usage};
        } catch (Failure failure) {
            return std::unexpected(failure);
        }
    }
    std::expected<Ticket, Failure> ReferenceHost::prepare(const PlanHandle& plan, const Request& request) {
        const auto checked = preview(plan, request);
        if (!checked)
            return std::unexpected(checked.error());
        if (tickets_issued_ == limits_.max_ticket)
            return std::unexpected(Failure{Error::TicketLimit});
        const auto serial = tickets_issued_ + 1;
        Prepared prepared{plan, request, state_.world.revision, serial};
        Ticket ticket{owner_, serial};
        prepared_ = std::move(prepared);
        tickets_issued_ = serial;
        return ticket;
    }
    const ReferenceHost::Prepared& ReferenceHost::authorize(const Ticket& ticket) const {
        if (!ticket.owner_)
            refuse(Error::InvalidTicket);
        if (ticket.owner_ != owner_)
            refuse(Error::ForeignTicket);
        if (!prepared_)
            refuse(Error::ConsumedTicket);
        if (ticket.serial_ != prepared_->serial)
            refuse(Error::StaleTicket);
        return *prepared_;
    }
    std::expected<void, Failure> ReferenceHost::discard(const Ticket& ticket) {
        try {
            static_cast<void>(authorize(ticket));
            prepared_.reset();
            return {};
        } catch (Failure failure) {
            return std::unexpected(failure);
        }
    }
    StepResult ReferenceHost::adopt(State&& next, Observation observation) {
        static_assert(std::is_nothrow_move_assignable_v<State>);
        static_assert(std::is_nothrow_move_constructible_v<StepResult>);
        auto report = std::make_shared<const Observation>(std::move(observation)); // allocate before adoption
        state_ = std::move(next);
        return Observed(std::move(report));
    }
    StepResult ReferenceHost::release(const Ticket& ticket) {
        try {
            const auto& input = authorize(ticket);
            Meter meter(limits_);
            meter.tick = state_.world.tick;
            validate(input.plan, input.request, state_, meter, true);
            if (input.revision != state_.world.revision)
                meter.fail(Error::StaleTicket);
            if (state_.world.actions_issued == limits_.max_action)
                meter.fail(Error::ActionLimit);
            auto next = state_;
            revision(next.world, limits_);
            const auto id = next.world.actions_issued + 1;
            meter.action = id;
            const auto due = later(next.world.tick, input.plan->meaning().onset_ticks, limits_);
            auto kind = EventKind::Applied;
            if (input.plan->meaning().onset_ticks != 0) {
                std::size_t slot = 0;
                while (slot < limits_.pending && next.world.pending[slot].present) {
                    meter.spend();
                    ++slot;
                }
                if (slot == limits_.pending)
                    meter.fail(Error::PendingLimit);
                next.world.pending[slot] = {true, id, due, input.request.caster, *input.request.aim, input.plan->metrics().retained_bytes};
                next.plans[slot] = input.plan;
                kind = EventKind::Queued;
            } else
                apply(input.plan, input.request, next, meter);
            next.world.actors[input.request.caster.slot].data.mana -= static_cast<std::uint32_t>(input.plan->meaning().effects.size() * 10);
            next.world.actions_issued = id;
            Observation observation{state_.world, next.world, {}, 1, meter.usage};
            observation.events[0] = {kind, id, next.world.tick};
            auto result = adopt(std::move(next), std::move(observation));
            prepared_.reset();
            return result;
        } catch (Failure failure) {
            failure.action = 0;
            failure.tick = state_.world.tick;
            return std::unexpected(failure);
        }
    }
    StepResult ReferenceHost::advance_to(std::uint64_t target) {
        std::uint64_t current_action = 0;
        std::uint64_t current_tick = state_.world.tick;
        try {
            Meter meter(limits_);
            meter.tick = state_.world.tick;
            if (target < state_.world.tick || target > limits_.max_tick)
                meter.fail(Error::TimeLimit);
            auto next = state_;
            if (target != next.world.tick)
                revision(next.world, limits_);
            Observation observation{};
            observation.before = state_.world;
            while (true) {
                std::optional<std::uint64_t> due;
                for (const auto& pending : next.world.pending) {
                    meter.spend();
                    if (pending.present && pending.due_tick <= target && (!due || pending.due_tick < *due))
                        due = pending.due_tick;
                }
                for (const auto& slot : next.world.actors) {
                    meter.spend();
                    if (slot.present && slot.slow.until_tick != 0 && slot.slow.until_tick <= target &&
                        (!due || slot.slow.until_tick < *due))
                        due = slot.slow.until_tick;
                }
                if (!due)
                    break;
                next.world.tick = *due;
                meter.tick = *due;
                current_tick = *due;
                current_action = 0;
                for (auto& slot : next.world.actors) {
                    meter.spend();
                    if (slot.present && slot.slow.until_tick != 0 && slot.slow.until_tick <= *due)
                        slot.slow = {};
                }
                while (true) {
                    std::optional<std::size_t> index;
                    for (std::size_t i = 0; i < pending_capacity; ++i) {
                        meter.spend();
                        const auto& p = next.world.pending[i];
                        if (p.present && p.due_tick == *due && (!index || p.action < next.world.pending[*index].action))
                            index = i;
                    }
                    if (!index)
                        break;
                    const auto p = next.world.pending[*index];
                    meter.action = p.action;
                    current_action = p.action;
                    const Request request{p.caster, std::nullopt, p.aim};
                    validate(next.plans[*index], request, next, meter, false);
                    apply(next.plans[*index], request, next, meter);
                    next.world.pending[*index] = {};
                    next.plans[*index].reset();
                    observation.events[observation.event_count++] = {EventKind::Activated, p.action, *due};
                }
            }
            next.world.tick = target;
            observation.after = next.world;
            observation.usage = meter.usage;
            return adopt(std::move(next), std::move(observation));
        } catch (Failure failure) {
            if (failure.action == 0)
                failure.action = current_action;
            failure.tick = current_tick;
            return std::unexpected(failure);
        }
    }
    StepResult ReferenceHost::cancel(std::uint64_t action) {
        try {
            Meter meter(limits_);
            meter.tick = state_.world.tick;
            meter.action = action;
            auto next = state_;
            for (std::size_t i = 0; i < pending_capacity; ++i) {
                meter.spend();
                if (!next.world.pending[i].present || next.world.pending[i].action != action)
                    continue;
                revision(next.world, limits_);
                next.world.pending[i] = {};
                next.plans[i].reset();
                Observation observation{state_.world, next.world, {}, 1, meter.usage};
                observation.events[0] = {EventKind::Cancelled, action, next.world.tick};
                return adopt(std::move(next), std::move(observation));
            }
            meter.fail(Error::MissingAction);
        } catch (Failure failure) {
            return std::unexpected(failure);
        }
    }

    std::string_view error_name(Error error) noexcept {
#define MAGIC_ERROR(value)                                                                                                                 \
    case Error::value:                                                                                                                     \
        return #value
        switch (error) {
            MAGIC_ERROR(InvalidLimits);
            MAGIC_ERROR(InvalidCatalog);
            MAGIC_ERROR(InvalidActor);
            MAGIC_ERROR(ActorLimit);
            MAGIC_ERROR(MissingActor);
            MAGIC_ERROR(GenerationLimit);
            MAGIC_ERROR(RevisionLimit);
            MAGIC_ERROR(NoPlan);
            MAGIC_ERROR(PlanLimit);
            MAGIC_ERROR(UnsupportedEntry);
            MAGIC_ERROR(UnsupportedDelivery);
            MAGIC_ERROR(UnsupportedBinding);
            MAGIC_ERROR(SchemaMismatch);
            MAGIC_ERROR(Permission);
            MAGIC_ERROR(Contact);
            MAGIC_ERROR(Resources);
            MAGIC_ERROR(InvalidAim);
            MAGIC_ERROR(EmptySelection);
            MAGIC_ERROR(Overlap);
            MAGIC_ERROR(ActiveSlow);
            MAGIC_ERROR(StatusLimit);
            MAGIC_ERROR(PendingLimit);
            MAGIC_ERROR(SelectorLimit);
            MAGIC_ERROR(TargetLimit);
            MAGIC_ERROR(WorkLimit);
            MAGIC_ERROR(TimeLimit);
            MAGIC_ERROR(ActionLimit);
            MAGIC_ERROR(TicketLimit);
            MAGIC_ERROR(InvalidTicket);
            MAGIC_ERROR(ForeignTicket);
            MAGIC_ERROR(StaleTicket);
            MAGIC_ERROR(ConsumedTicket);
            MAGIC_ERROR(MissingAction);
        }
#undef MAGIC_ERROR
        return "InvalidError";
    }
}
