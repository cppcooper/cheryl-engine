#include <magic/reference-host.h>
#include "reference-meter.h"
#include <algorithm>

namespace Magic::Reference {
    std::expected<SlowSnapshot, Failure> ReferenceHost::slows() const {
        try {
            Meter meter(limits_);
            meter.spend(slow_capacity);
            return state_.slows;
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    void ReferenceHost::refresh_slow(State& state, ActorId actor, Meter& meter) const {
        Slow combined;
        for (const auto& slow : state.slows.influences) {
            meter.spend();
            if (slow.instance && slow.actor == actor && slow.until_tick > state.world.tick) {
                combined.permille = std::min(combined.permille, slow.permille);
                combined.until_tick = combined.until_tick ? std::min(combined.until_tick, slow.until_tick) : slow.until_tick;
            }
        }
        state.world.actors[actor.slot].slow = combined;
    }
    void ReferenceHost::add_slow(
        const PlanHandle& plan,
        std::size_t effect,
        ActorId actor,
        SlowSource source,
        State& state,
        Meter& meter
    ) const {
        if (state.slows.instances_issued == limits_.max_slow_instance)
            meter.fail(Error::InstanceLimit);
        const auto& e = plan->meaning().effects[effect];
        if (e.duration_ticks > limits_.max_tick - state.world.tick)
            meter.fail(Error::TimeLimit);
        for (std::size_t i = 0; i < limits_.statuses; ++i) {
            meter.spend();
            auto& slow = state.slows.influences[i];
            if (slow.instance)
                continue;
            slow = {
                ++state.slows.instances_issued, actor, source, effect, e.quantity, state.world.tick, state.world.tick + e.duration_ticks,
                plan->metrics().retained_bytes
            };
            state.slow_plans[i] = plan;
            auto& combined = state.world.actors[actor.slot].slow;
            combined.permille = std::min(combined.permille, slow.permille);
            combined.until_tick = combined.until_tick ? std::min(combined.until_tick, slow.until_tick) : slow.until_tick;
            return;
        }
        meter.fail(Error::StatusLimit);
    }
    std::expected<InfluenceRef, Failure> ReferenceHost::bind_slow(std::uint64_t instance, ActorId actor) const {
        try {
            Meter meter(limits_);
            for (const auto& slow : state_.slows.influences) {
                meter.spend();
                if (instance && slow.instance == instance && slow.actor == actor && slow.until_tick > state_.world.tick) {
                    InfluenceRef ref;
                    ref.owner_ = owner_;
                    ref.kind_ = InfluenceKind::Slow;
                    ref.instance_ = instance;
                    ref.actor_ = actor;
                    return ref;
                }
            }
            meter.fail(Error::MissingInfluence);
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    void ReferenceHost::check_slows(const State& state, Meter& meter) const {
        const auto check = [&](bool valid) {
            if (!valid)
                meter.fail(Error::InvalidCheckpoint);
        };
        check(state.slows.version == 1 && state.slows.instances_issued <= limits_.max_slow_instance);
        std::array<Slow, actor_capacity> combined{};
        for (std::size_t i = 0; i < slow_capacity; ++i) {
            meter.spend();
            const auto& slow = state.slows.influences[i];
            const auto& plan = state.slow_plans[i];
            if (!slow.instance) {
                check(slow == SlowInfluence{} && !plan);
                continue;
            }
            check(
                i < limits_.statuses && slow.instance <= state.slows.instances_issued && slow.actor.slot < limits_.actors &&
                slow.actor.generation && state.world.actors[slow.actor.slot].present &&
                state.world.actors[slow.actor.slot].id == slow.actor && slow.onset_tick <= state.world.tick &&
                slow.until_tick > state.world.tick && slow.until_tick <= limits_.max_tick && plan
            );
            payload_schema(plan, meter);
            const auto& m = plan->meaning();
            check(slow.plan_bytes == plan->metrics().retained_bytes && slow.effect_index < m.effects.size());
            const auto& e = m.effects[slow.effect_index];
            check(
                e.primitive == Primitive::Slow && e.quantity == slow.permille && e.duration_ticks == slow.until_tick - slow.onset_tick &&
                e.binding < m.bindings.size()
            );
            const auto& source = slow.source;
            if (source.item) {
                check(
                    m.entry.kind == EntryKind::Engraved && source.caster == ActorId{} && source.action == 0 &&
                    source.item->slot < limits_.items && source.item->generation &&
                    source.item->generation <= state.items.items[source.item->slot].id.generation && source.fact
                );
                bool found = false;
                for (const auto& fact : state.items.ledger) {
                    meter.spend();
                    found |= fact.fact == source.fact && fact.item == *source.item && fact.toucher == slow.actor && fact.consumed;
                }
                check(found);
                inscription(plan, meter);
            } else {
                check(
                    m.entry.kind == EntryKind::Spoken && source.fact == 0 && source.action && source.action <= state.world.actions_issued &&
                    source.caster.slot < limits_.actors && source.caster.generation &&
                    source.caster.generation <= state.world.actors[source.caster.slot].id.generation &&
                    ((m.delivery == Delivery::Contact && catalog_.contact) || (m.delivery == Delivery::Burst && catalog_.burst)) &&
                    catalog_.spoken
                );
            }
            for (std::size_t j = 0; j < i; ++j) {
                meter.spend();
                const auto& old = state.slows.influences[j];
                check(
                    !old.instance || (old.instance != slow.instance &&
                                      (old.source != source || old.actor != slow.actor || old.effect_index != slow.effect_index))
                );
            }
            auto& aggregate = combined[slow.actor.slot];
            aggregate.permille = std::min(aggregate.permille, slow.permille);
            aggregate.until_tick = aggregate.until_tick ? std::min(aggregate.until_tick, slow.until_tick) : slow.until_tick;
        }
        for (std::size_t i = 0; i < actor_capacity; ++i) {
            meter.spend();
            check(state.world.actors[i].slow == combined[i]);
        }
    }
}
