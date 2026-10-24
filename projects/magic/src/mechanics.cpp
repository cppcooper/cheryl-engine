#include <magic/reference-host.h>
#include "reference-meter.h"

#include <algorithm>
#include <type_traits>

namespace Magic::Reference {
    namespace {
        const ActorView& live(const Snapshot& world, ActorId id) {
            if (id.slot >= actor_capacity || !id.generation || !world.actors[id.slot].present || world.actors[id.slot].id != id)
                throw Failure{Error::MissingActor};
            return world.actors[id.slot];
        }
        std::uint64_t expiry(std::uint64_t now, std::uint32_t duration, const HostLimits& limits) {
            if (duration > limits.max_tick - now)
                throw Failure{Error::TimeLimit};
            return now + duration;
        }
        void revise(Snapshot& world, const HostLimits& limits) {
            if (world.revision == limits.max_revision)
                throw Failure{Error::RevisionLimit};
            ++world.revision;
        }
    }
    std::expected<MechanicsSnapshot, Failure> ReferenceHost::inspect_mechanics() const {
        try {
            Meter meter(limits_);
            meter.spend(8);
            return state_.mechanics;
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    DamageResolution ReferenceHost::damage(ActorId target, std::uint32_t amount, State& state, Meter& meter) const {
        static_cast<void>(live(state.world, target));
        DamageResolution result{amount, 0, 0};
        for (std::size_t i = 0; i < 4; ++i) {
            meter.spend();
            auto& ward = state.mechanics.wards[i];
            if (!ward.instance || ward.actor != target)
                continue;
            result.prevented_dp = std::min(amount, ward.remaining_dp);
            ward.remaining_dp -= result.prevented_dp;
            if (!ward.remaining_dp) {
                ward = {};
                state.ward_plans[i].reset();
            }
        }
        auto& health = state.world.actors[target.slot].data.health;
        result.health_lost = std::min(health, amount - result.prevented_dp);
        health -= result.health_lost;
        return result;
    }
    std::expected<AttackObserved, Failure> ReferenceHost::try_attack(ActorId attacker, ActorId target) {
        try {
            Meter meter(limits_);
            const auto& a = live(state_.world, attacker);
            const auto& t = live(state_.world, target);
            if (!a.data.health || !t.data.health || attacker == target)
                meter.fail(Error::Permission);
            const auto dx = static_cast<std::int64_t>(a.data.position.x) - t.data.position.x;
            const auto dy = static_cast<std::int64_t>(a.data.position.y) - t.data.position.y;
            // Positions are bounded +/-1e6; each square<=4e12 and sum<=8e12 fits signed64.
            meter.spend(3);
            if (dx * dx + dy * dy > 2500)
                meter.fail(Error::Contact);
            for (const auto& r : state_.mechanics.restrictions) {
                meter.spend();
                if (r.instance && r.actor == attacker && r.intent == Intent::Attack)
                    meter.fail(Error::RestrictedIntent);
            }
            meter.select();
            auto next = state_;
            revise(next.world, limits_);
            auto resolution = damage(target, 3, next, meter);
            meter.spend(16); // fixed snapshots and owned outcome
            auto result = std::make_shared<const AttackObservation>(
                AttackObservation{state_.world, next.world, state_.mechanics, next.mechanics, attacker, target, resolution}
            );
            state_ = std::move(next);
            return result;
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    void
    ReferenceHost::mechanical(const PlanHandle& plan, const std::array<bool, actor_capacity>& selected, State& state, Meter& meter) const {
        const auto& effect = plan->meaning().effects[0];
        const auto duration = effect_duration_ticks(effect);
        const auto until = expiry(state.world.tick, duration, limits_);
        for (std::size_t a = 0; a < actor_capacity; ++a) {
            meter.spend();
            if (!selected[a])
                continue;
            const auto id = state.world.actors[a].id;
            if (state.mechanics.instances_issued == limits_.max_mechanic_instance)
                meter.fail(Error::MechanicLimit);
            std::optional<std::size_t> free;
            for (std::size_t i = 0; i < 4; ++i) {
                meter.spend();
                if (effect.primitive == Primitive::Ward) {
                    const auto& w = state.mechanics.wards[i];
                    if (w.instance && w.actor == id)
                        meter.fail(Error::ActiveWard);
                    if (!w.instance && i < limits_.wards && !free)
                        free = i;
                } else {
                    const auto& r = state.mechanics.restrictions[i];
                    if (r.instance && r.actor == id)
                        meter.fail(Error::Overlap);
                    if (!r.instance && i < limits_.restrictions && !free)
                        free = i;
                }
            }
            if (!free)
                meter.fail(Error::MechanicLimit);
            const auto instance = ++state.mechanics.instances_issued;
            if (const auto* w = std::get_if<Absorption>(&effect.payload)) {
                state.mechanics.wards[*free] = {instance, meter.action, id, w->capacity_dp, until, state.world.tick};
                state.ward_plans[*free] = plan;
            } else {
                state.mechanics.restrictions[*free] = {instance, meter.action, id, Intent::Attack, until, state.world.tick};
                state.restriction_plans[*free] = plan;
            }
        }
    }
    std::optional<std::uint64_t> ReferenceHost::mechanic_due(const State& state, std::uint64_t target, Meter& meter) const {
        std::optional<std::uint64_t> due;
        auto visit = [&](const auto& list) {
            for (const auto& r : list) {
                meter.spend();
                if (r.instance && r.until_tick <= target && (!due || r.until_tick < *due))
                    due = r.until_tick;
            }
        };
        visit(state.mechanics.wards);
        visit(state.mechanics.restrictions);
        return due;
    }
    void ReferenceHost::expire_mechanics(State& state, std::uint64_t tick, Meter& meter) const {
        for (std::size_t i = 0; i < 4; ++i) {
            meter.spend(2);
            if (state.mechanics.wards[i].instance && state.mechanics.wards[i].until_tick <= tick) {
                state.mechanics.wards[i] = {};
                state.ward_plans[i].reset();
            }
            if (state.mechanics.restrictions[i].instance && state.mechanics.restrictions[i].until_tick <= tick) {
                state.mechanics.restrictions[i] = {};
                state.restriction_plans[i].reset();
            }
        }
    }
    void ReferenceHost::forget_actor_mechanics(State& state, ActorId id, Meter& meter) const {
        for (std::size_t i = 0; i < 4; ++i) {
            meter.spend(2);
            if (state.mechanics.wards[i].instance && state.mechanics.wards[i].actor == id) {
                state.mechanics.wards[i] = {};
                state.ward_plans[i].reset();
            }
            if (state.mechanics.restrictions[i].instance && state.mechanics.restrictions[i].actor == id) {
                state.mechanics.restrictions[i] = {};
                state.restriction_plans[i].reset();
            }
        }
    }
    void ReferenceHost::check_mechanics(const State& state, Meter& meter) const {
        const auto& m = state.mechanics;
        auto check = [&](bool value) {
            if (!value)
                meter.fail(Error::InvalidCheckpoint);
        };
        check(m.version == 1 && m.instances_issued <= limits_.max_mechanic_instance);
        for (std::size_t i = 0; i < 4; ++i) {
            meter.spend(2);
            const auto& w = m.wards[i];
            const auto& r = m.restrictions[i];
            auto common = [&](const auto& v, const PlanHandle& plan, std::size_t cap, Primitive p) {
                check(
                    i < cap && v.instance <= m.instances_issued && v.action && v.action <= state.world.actions_issued &&
                    v.until_tick > state.world.tick && v.until_tick <= limits_.max_tick
                );
                static_cast<void>(live(state.world, v.actor));
                check(plan && plan->meaning().effects.size() == 1 && plan->meaning().effects[0].primitive == p);
                payload_schema(plan, meter);
                check(v.onset_tick <= state.world.tick && v.until_tick - v.onset_tick == effect_duration_ticks(plan->meaning().effects[0]));
            };
            if (w.instance) {
                common(w, state.ward_plans[i], limits_.wards, Primitive::Ward);
                check(
                    w.remaining_dp > 0 &&
                    w.remaining_dp <= std::get<Absorption>(state.ward_plans[i]->meaning().effects[0].payload).capacity_dp
                );
            } else
                check(w == WardView{} && !state.ward_plans[i]);
            if (r.instance) {
                common(r, state.restriction_plans[i], limits_.restrictions, Primitive::Restrict);
                check(r.intent == Intent::Attack);
            } else
                check(r == RestrictionView{} && !state.restriction_plans[i]);
            for (std::size_t j = 0; j < i; ++j) {
                meter.spend(4);
                if (w.instance)
                    check(
                        (!m.wards[j].instance || (m.wards[j].instance != w.instance && m.wards[j].actor != w.actor)) &&
                        (!m.restrictions[j].instance || m.restrictions[j].instance != w.instance)
                    );
                if (r.instance)
                    check(
                        (!m.restrictions[j].instance || (m.restrictions[j].instance != r.instance && m.restrictions[j].actor != r.actor)) &&
                        (!m.wards[j].instance || m.wards[j].instance != r.instance)
                    );
            }
            if (w.instance && r.instance)
                check(w.instance != r.instance);
        }
    }
}
