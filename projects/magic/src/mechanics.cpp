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
    void ReferenceHost::override_material(State& state, ObjectId subject, Meter& meter) const {
        for (std::size_t i = 0; i < 4; ++i) {
            meter.spend();
            if (state.mechanics.transmutations[i].instance && state.mechanics.transmutations[i].subject == subject) {
                state.mechanics.transmutations[i] = {};
                state.transmutation_plans[i].reset();
            }
        }
    }
    void ReferenceHost::transmute(const PlanHandle& plan, ObjectId subject, State& state, Meter& meter) const {
        if (subject.slot >= object_capacity || !subject.generation || !state.perception.objects[subject.slot].present ||
            state.perception.objects[subject.slot].id != subject)
            meter.fail(Error::MissingObject);
        const auto& payload = std::get<Transmutation>(plan->meaning().effects[0].payload);
        const auto until = expiry(state.world.tick, payload.duration_ticks, limits_);
        if (state.mechanics.instances_issued == limits_.max_mechanic_instance)
            meter.fail(Error::MechanicLimit);
        std::optional<std::size_t> free;
        for (std::size_t i = 0; i < 4; ++i) {
            meter.spend();
            const auto& t = state.mechanics.transmutations[i];
            if (t.instance && t.subject == subject)
                meter.fail(Error::Overlap);
            if (!t.instance && i < limits_.transmutations && !free)
                free = i;
        }
        if (!free)
            meter.fail(Error::MechanicLimit);
        auto& object = state.perception.objects[subject.slot];
        const auto id = ++state.mechanics.instances_issued;
        state.mechanics.transmutations[*free] = {id,           meter.action,     subject, object.data.form, object.data.passable,
                                                 payload.form, state.world.tick, until};
        state.transmutation_plans[*free] = plan;
        object.data.form = payload.form;
        object.data.passable = payload.form == Appearance::OpenArch;
        object.fact_revision = state.world.revision;
    }
    void ReferenceHost::create_object(const PlanHandle& plan, ActorId owner, State& state, Meter& meter) const {
        static_cast<void>(live(state.world, owner));
        const auto& payload = std::get<Creation>(plan->meaning().effects[0].payload);
        const auto until = expiry(state.world.tick, payload.duration_ticks, limits_);
        if (state.mechanics.instances_issued == limits_.max_mechanic_instance)
            meter.fail(Error::MechanicLimit);
        for (const auto& a : state.world.actors) {
            meter.visit();
            if (a.present && a.data.position == payload.position)
                meter.fail(Error::DestinationBlocked);
        }
        for (const auto& o : state.perception.objects) {
            meter.visit();
            if (o.present && o.data.position == payload.position)
                meter.fail(Error::DestinationBlocked);
        }
        std::optional<std::size_t> free, object_slot;
        for (std::size_t i = 0; i < 4; ++i) {
            meter.spend(2);
            if (i < limits_.creations && !state.mechanics.creations[i].instance && !free)
                free = i;
            if (i < limits_.objects && !state.perception.objects[i].present && !object_slot)
                object_slot = i;
        }
        if (!free)
            meter.fail(Error::MechanicLimit);
        if (!object_slot)
            meter.fail(Error::ObjectLimit);
        auto& object = state.perception.objects[*object_slot];
        if (object.id.generation == limits_.max_generation)
            meter.fail(Error::GenerationLimit);
        ++object.id.generation;
        object.present = true;
        object.data = {payload.form, payload.form == Appearance::OpenArch, payload.position};
        object.fact_revision = state.world.revision;
        const auto id = ++state.mechanics.instances_issued;
        state.mechanics.creations[*free] = {id, meter.action, object.id, owner, state.world.tick, until};
        state.creation_plans[*free] = plan;
    }
    std::expected<void, Failure> ReferenceHost::dismiss_created(ActorId owner, ObjectId object) {
        try {
            Meter meter(limits_);
            const auto& actor = live(state_.world, owner);
            if (!actor.data.health)
                meter.fail(Error::Permission);
            for (const auto& c : state_.mechanics.creations) {
                meter.spend();
                if (c.instance && c.object == object) {
                    if (c.owner != owner)
                        meter.fail(Error::Permission);
                    auto next = state_;
                    revise(next.world, limits_);
                    erase_object(next, object, meter);
                    state_ = std::move(next);
                    return {};
                }
            }
            meter.fail(Error::MissingObject);
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }

    void ReferenceHost::erase_object(State& state, ObjectId subject, Meter& meter) const {
        if (subject.slot >= object_capacity || !subject.generation || !state.perception.objects[subject.slot].present ||
            state.perception.objects[subject.slot].id != subject)
            meter.fail(Error::MissingObject);
        override_material(state, subject, meter);
        for (std::size_t i = 0; i < 4; ++i) {
            meter.spend();
            if (state.mechanics.creations[i].instance && state.mechanics.creations[i].object == subject) {
                state.mechanics.creations[i] = {};
                state.creation_plans[i].reset();
            }
        }
        state.perception.objects[subject.slot] = ObjectView{subject};
        for (std::size_t i = 0; i < disguise_capacity; ++i) {
            meter.spend();
            if (state.perception.disguises[i].instance && state.perception.disguises[i].subject == subject) {
                state.perception.disguises[i] = {};
                state.disguise_plans[i].reset();
            }
        }
    }
    void ReferenceHost::relocate(const Relocation& payload, ActorId target, State& state, Meter& meter) const {
        static_cast<void>(live(state.world, target));
        for (const auto& actor : state.world.actors) {
            meter.visit();
            if (actor.present && actor.id != target && actor.data.position == payload.destination)
                meter.fail(Error::DestinationBlocked);
        }
        for (const auto& object : state.perception.objects) {
            meter.visit();
            if (object.present && !object.data.passable && object.data.position == payload.destination)
                meter.fail(Error::DestinationBlocked);
        }
        state.world.actors[target.slot].data.position = payload.destination;
    }
    std::expected<MechanicsSnapshot, Failure> ReferenceHost::inspect_mechanics() const {
        try {
            Meter meter(limits_);
            meter.spend(16);
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
        visit(state.mechanics.transmutations);
        visit(state.mechanics.creations);
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
        for (std::size_t i = 0; i < 4; ++i) {
            meter.spend(2);
            const auto t = state.mechanics.transmutations[i];
            if (t.instance && t.until_tick <= tick) {
                auto& object = state.perception.objects[t.subject.slot];
                if (object.present && object.id == t.subject) {
                    object.data.form = t.original_form;
                    object.data.passable = t.original_passable;
                    object.fact_revision = state.world.revision;
                }
                state.mechanics.transmutations[i] = {};
                state.transmutation_plans[i].reset();
            }
            const auto c = state.mechanics.creations[i];
            if (c.instance && c.until_tick <= tick) {
                if (state.perception.objects[c.object.slot].present && state.perception.objects[c.object.slot].id == c.object)
                    erase_object(state, c.object, meter);
                else {
                    state.mechanics.creations[i] = {};
                    state.creation_plans[i].reset();
                }
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
                check(
                    catalog_.spoken && ((plan->meaning().delivery == Delivery::Contact && catalog_.contact) ||
                                        (plan->meaning().delivery == Delivery::Burst && catalog_.burst))
                );
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
        std::array<std::uint64_t, 16> identities{};
        std::size_t count = 0;
        auto identity = [&](std::uint64_t id) {
            meter.spend();
            if (id)
                identities[count++] = id;
        };
        for (const auto& w : m.wards)
            identity(w.instance);
        for (const auto& r : m.restrictions)
            identity(r.instance);
        for (std::size_t i = 0; i < 4; ++i) {
            meter.spend(2);
            const auto& t = m.transmutations[i];
            const auto& c = m.creations[i];
            identity(t.instance);
            identity(c.instance);
            auto common = [&](const auto& v, const PlanHandle& plan, Primitive primitive, std::size_t cap) {
                check(
                    i < cap && v.instance <= m.instances_issued && v.action && v.action <= state.world.actions_issued &&
                    v.onset_tick <= state.world.tick && v.until_tick > state.world.tick && v.until_tick <= limits_.max_tick
                );
                check(
                    plan && plan->meaning().entry.kind == EntryKind::Spoken && plan->meaning().effects.size() == 1 &&
                    plan->meaning().effects[0].primitive == primitive
                );
                payload_schema(plan, meter);
                check(v.until_tick - v.onset_tick == effect_duration_ticks(plan->meaning().effects[0]));
            };
            if (t.instance) {
                common(t, state.transmutation_plans[i], Primitive::Transmute, limits_.transmutations);
                check(catalog_.spoken && catalog_.subject && t.subject.slot < limits_.objects);
                const auto& o = state.perception.objects[t.subject.slot];
                check(o.present && o.id == t.subject && o.data.form == t.form && o.data.passable == (t.form == Appearance::OpenArch));
                check(
                    (t.original_form == Appearance::OpenArch || t.original_form == Appearance::StoneWall) &&
                    std::get<Transmutation>(state.transmutation_plans[i]->meaning().effects[0].payload).form == t.form
                );
                for (std::size_t j = 0; j < i; ++j) {
                    meter.spend();
                    check(!m.transmutations[j].instance || m.transmutations[j].subject != t.subject);
                }
            } else
                check(t == TransmutationView{} && !state.transmutation_plans[i]);
            if (c.instance) {
                common(c, state.creation_plans[i], Primitive::Create, limits_.creations);
                check(catalog_.spoken && catalog_.contact && c.object.slot < limits_.objects);
                const auto& o = state.perception.objects[c.object.slot];
                check(o.present && o.id == c.object);
                check(
                    c.owner.slot < limits_.actors && c.owner.generation &&
                    c.owner.generation <= state.world.actors[c.owner.slot].id.generation
                );
                for (std::size_t j = 0; j < i; ++j) {
                    meter.spend();
                    check(!m.creations[j].instance || m.creations[j].object != c.object);
                }
            } else
                check(c == CreationView{} && !state.creation_plans[i]);
        }
        for (std::size_t i = 0; i < count; ++i)
            for (std::size_t j = 0; j < i; ++j) {
                meter.spend();
                check(identities[i] != identities[j]);
            }
    }
}
