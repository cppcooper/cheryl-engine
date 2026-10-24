#include <magic/reference-host.h>
#include "reference-meter.h"

namespace Magic::Reference {
    namespace {
        bool valid(Point p) {
            return p.x >= -1'000'000 && p.x <= 1'000'000 && p.y >= -1'000'000 && p.y <= 1'000'000;
        }
        const ActorView& live(const Snapshot& world, ActorId id) {
            if (id.slot >= actor_capacity || !id.generation || !world.actors[id.slot].present || world.actors[id.slot].id != id)
                throw Failure{Error::MissingActor};
            return world.actors[id.slot];
        }
        void revise(Snapshot& world, const HostLimits& limits) {
            if (world.revision == limits.max_revision)
                throw Failure{Error::RevisionLimit};
            ++world.revision;
        }
    }
    std::expected<LifeSnapshot, Failure> ReferenceHost::life() const {
        try {
            Meter meter(limits_);
            meter.spend(8);
            return state_.life;
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<RemainsId, Failure> ReferenceHost::add_remains(Point position) {
        try {
            Meter meter(limits_);
            if (!valid(position))
                meter.fail(Error::InvalidAim);
            auto next = state_;
            for (std::size_t i = 0; i < limits_.remains; ++i) {
                meter.spend();
                auto& r = next.life.remains[i];
                if (r.present)
                    continue;
                if (r.id.generation == limits_.max_generation)
                    meter.fail(Error::GenerationLimit);
                revise(next.world, limits_);
                r = {{static_cast<std::uint32_t>(i), r.id.generation + 1}, true, false, position};
                const auto id = r.id;
                state_ = std::move(next);
                return id;
            }
            meter.fail(Error::RemainsLimit);
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<void, Failure> ReferenceHost::remove_remains(RemainsId id) {
        try {
            Meter meter(limits_);
            if (id.slot >= 4 || !id.generation || !state_.life.remains[id.slot].present || state_.life.remains[id.slot].id != id)
                meter.fail(Error::MissingRemains);
            for (const auto& s : state_.life.servants) {
                meter.spend();
                if (s.instance && s.remains == id)
                    meter.fail(Error::ServantLimit);
            }
            auto next = state_;
            revise(next.world, limits_);
            next.life.remains[id.slot] = {id, false, false, {}};
            state_ = std::move(next);
            return {};
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    void
    ReferenceHost::validate_lifecycle(const PlanHandle& plan, const Request& request, const State& state, Meter& meter, bool paying) const {
        const auto& m = plan->meaning();
        if (!catalog_.spoken || !catalog_.subject)
            meter.fail(Error::SchemaMismatch);
        if (m.entry.kind != EntryKind::Spoken || m.entry.charges || m.entry.trigger != Trigger::None ||
            m.entry.mode != CastingMode::Release)
            meter.fail(Error::UnsupportedEntry);
        if (m.onset_ticks || m.effects.size() != 1 || m.bindings.size() != 1)
            meter.fail(Error::UnsupportedComposition);
        if (request.contact || request.aim || request.subject)
            meter.fail(Error::UnsupportedBinding);
        const auto& caster = live(state.world, request.caster);
        if (!caster.data.can_cast || !caster.data.health)
            meter.fail(Error::Permission);
        payload_schema(plan, meter);
        const auto& b = m.bindings[0];
        meter.spend(b.name.size() + b.frame.size() + 1);
        if (!b.frame.empty() || b.radius_mm || b.inner_radius_mm || !b.inclusive || b.anchor_time != BindingTime::Release ||
            b.query_time != BindingTime::Release || m.effects[0].binding)
            meter.fail(Error::UnsupportedBinding);
        if (m.delivery == Delivery::Pending) {
            if (m.effects[0].primitive != Primitive::Interrupt || b.query != Query::CapturedPending || request.remains ||
                !request.influence.empty())
                meter.fail(Error::UnsupportedBinding);
            const auto& ref = request.pending;
            if (!ref.owner_)
                meter.fail(Error::InvalidCapability);
            if (ref.owner_ != owner_)
                meter.fail(Error::ForeignCapability);
            meter.spend(2); // owned action/caster identity input and comparison
            bool found = false;
            for (const auto& p : state.world.pending) {
                meter.spend();
                found |= p.present && p.action == ref.action_ && p.caster == ref.caster_;
            }
            if (!found)
                meter.fail(Error::MissingAction);
        } else if (!request.pending.empty())
            meter.fail(Error::UnsupportedBinding);
        else if (m.delivery == Delivery::Remains) {
            if (m.effects[0].primitive != Primitive::Animate || b.query != Query::CapturedRemains || !request.remains ||
                !request.influence.empty())
                meter.fail(Error::UnsupportedBinding);
            const auto id = *request.remains;
            if (id.slot >= 4 || !id.generation || !state.life.remains[id.slot].present || state.life.remains[id.slot].id != id)
                meter.fail(Error::MissingRemains);
            const auto& r = state.life.remains[id.slot];
            if (r.spent)
                meter.fail(Error::SpentRemains);
            const auto dx = static_cast<std::int64_t>(r.position.x) - caster.data.position.x;
            const auto dy = static_cast<std::int64_t>(r.position.y) - caster.data.position.y;
            meter.spend(3); // bounded G0 differences <=2e6; squares/sum fit int64
            if (dx * dx + dy * dy > 2500)
                meter.fail(Error::Contact);
            for (const auto& schema : catalog_.operations) {
                meter.spend();
                if (schema.primitive == Primitive::Animate && schema.key == m.effects[0].key) {
                    const auto bound = static_cast<std::int32_t>(schema.max_coordinate_mm);
                    if (r.position.x < -bound || r.position.x > bound || r.position.y < -bound || r.position.y > bound)
                        meter.fail(Error::SchemaMismatch);
                }
            }
        } else {
            if (m.effects[0].primitive != Primitive::Dispel || b.query != Query::CapturedInfluence || request.remains)
                meter.fail(Error::UnsupportedBinding);
            const auto& ref = request.influence;
            if (!ref.owner_)
                meter.fail(Error::InvalidCapability);
            if (ref.owner_ != owner_)
                meter.fail(Error::ForeignCapability);
            if (ref.kind_ != std::get<Dispelling>(m.effects[0].payload).kind)
                meter.fail(Error::UnsupportedBinding);
            bool found = false;
            if (ref.kind_ == InfluenceKind::Disguise) {
                for (const auto& d : state.perception.disguises) {
                    meter.spend();
                    found |= d.instance && d.instance == ref.instance_ && d.subject == ref.object_ && d.until_tick > state.world.tick;
                }
            } else {
                for (const auto& r : state.mechanics.restrictions) {
                    meter.spend();
                    found |= r.instance && r.instance == ref.instance_ && r.actor == ref.actor_ && r.until_tick > state.world.tick;
                }
            }
            if (!found)
                meter.fail(Error::MissingInfluence);
        }
        if (paying && caster.data.mana < 10)
            meter.fail(Error::Resources);
    }
    void ReferenceHost::lifecycle(const PlanHandle& plan, const Request& request, State& state, Meter& meter) const {
        meter.select();
        if (plan->meaning().delivery == Delivery::Pending) {
            const auto& ref = request.pending;
            for (std::size_t i = 0; i < pending_capacity; ++i) {
                meter.spend();
                if (state.world.pending[i].present && state.world.pending[i].action == ref.action_ &&
                    state.world.pending[i].caster == ref.caster_) {
                    state.world.pending[i] = {};
                    state.plans[i].reset();
                    return;
                }
            }
            meter.fail(Error::MissingAction);
        }
        if (plan->meaning().delivery == Delivery::Influence) {
            const auto& ref = request.influence;
            if (ref.kind_ == InfluenceKind::Disguise) {
                for (std::size_t i = 0; i < disguise_capacity; ++i) {
                    meter.spend();
                    if (state.perception.disguises[i].instance == ref.instance_ && state.perception.disguises[i].subject == ref.object_) {
                        state.perception.disguises[i] = {};
                        state.disguise_plans[i].reset();
                        return;
                    }
                }
            } else {
                for (std::size_t i = 0; i < 4; ++i) {
                    meter.spend();
                    if (state.mechanics.restrictions[i].instance == ref.instance_ && state.mechanics.restrictions[i].actor == ref.actor_) {
                        state.mechanics.restrictions[i] = {};
                        state.restriction_plans[i].reset();
                        return;
                    }
                }
            }
            meter.fail(Error::MissingInfluence);
        }
        auto& remains = state.life.remains[request.remains->slot];
        const auto duration = std::get<Animation>(plan->meaning().effects[0].payload).duration_ticks;
        if (duration > limits_.max_tick - state.world.tick)
            meter.fail(Error::TimeLimit);
        if (state.life.instances_issued == limits_.max_servant_instance)
            meter.fail(Error::InstanceLimit);
        std::optional<std::size_t> record, actor;
        for (std::size_t i = 0; i < 4; ++i) {
            meter.spend();
            if (!state.life.servants[i].instance && i < limits_.servants && !record)
                record = i;
        }
        if (!record)
            meter.fail(Error::ServantLimit);
        for (std::size_t i = 0; i < actor_capacity; ++i) {
            meter.visit();
            const auto& a = state.world.actors[i];
            if (a.present && a.data.position == remains.position)
                meter.fail(Error::DestinationBlocked);
            if (!a.present && i < limits_.actors && !actor)
                actor = i;
        }
        for (const auto& o : state.perception.objects) {
            meter.visit();
            if (o.present && !o.data.passable && o.data.position == remains.position)
                meter.fail(Error::DestinationBlocked);
        }
        if (!actor)
            meter.fail(Error::ActorLimit);
        auto& a = state.world.actors[*actor];
        if (a.id.generation == limits_.max_generation)
            meter.fail(Error::GenerationLimit);
        ++a.id.generation;
        a.present = true;
        a.data = {remains.position, 20, 20, 0, 1000, true, true, false};
        a.slow = {};
        const auto instance = ++state.life.instances_issued;
        state.life.servants[*record] = {
            instance, meter.action, a.id, request.caster, remains.id, state.world.tick, state.world.tick + duration
        };
        state.servant_plans[*record] = plan;
        remains.spent = true;
    }
    const ServantView& ReferenceHost::command_live(ActorId owner, ActorId servant, Meter& meter) const {
        meter.spend(2);
        if (!live(state_.world, owner).data.health || !live(state_.world, servant).data.health)
            meter.fail(Error::Permission);
        for (const auto& s : state_.life.servants) {
            meter.spend();
            if (s.instance && s.actor == servant && s.owner == owner && s.until_tick > state_.world.tick)
                return s;
        }
        meter.fail(Error::Permission);
    }
    std::expected<ServantCommand, Failure> ReferenceHost::command_authority(ActorId owner, ActorId servant) const {
        try {
            Meter meter(limits_);
            const auto& s = command_live(owner, servant, meter);
            ServantCommand c;
            c.incarnation_ = owner_;
            c.owner_ = owner;
            c.servant_ = servant;
            c.instance_ = s.instance;
            return c;
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<AttackObserved, Failure> ReferenceHost::command_attack(const ServantCommand& command, ActorId target) {
        try {
            Meter meter(limits_);
            if (!command.incarnation_)
                meter.fail(Error::InvalidCapability);
            if (command.incarnation_ != owner_)
                meter.fail(Error::ForeignCapability);
            const auto& s = command_live(command.owner_, command.servant_, meter);
            if (s.instance != command.instance_)
                meter.fail(Error::InvalidCapability);
            return attack(command.servant_, target, meter);
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<void, Failure> ReferenceHost::dismiss_servant(const ServantCommand& command) {
        try {
            Meter meter(limits_);
            if (!command.incarnation_)
                meter.fail(Error::InvalidCapability);
            if (command.incarnation_ != owner_)
                meter.fail(Error::ForeignCapability);
            const auto& s = command_live(command.owner_, command.servant_, meter);
            if (s.instance != command.instance_)
                meter.fail(Error::InvalidCapability);
            auto next = state_;
            revise(next.world, limits_);
            retire_actor(next, command.servant_, meter);
            state_ = std::move(next);
            return {};
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<PendingRef, Failure> ReferenceHost::bind_pending(std::uint64_t action) const {
        try {
            Meter meter(limits_);
            for (const auto& p : state_.world.pending) {
                meter.spend();
                if (p.present && action && p.action == action) {
                    PendingRef r;
                    r.owner_ = owner_;
                    r.action_ = action;
                    r.caster_ = p.caster;
                    return r;
                }
            }
            meter.fail(Error::MissingAction);
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<InfluenceRef, Failure> ReferenceHost::bind_disguise(std::uint64_t instance, ObjectId subject) const {
        try {
            Meter meter(limits_);
            for (const auto& d : state_.perception.disguises) {
                meter.spend();
                if (instance && d.instance == instance && d.subject == subject && d.until_tick > state_.world.tick) {
                    InfluenceRef r;
                    r.owner_ = owner_;
                    r.kind_ = InfluenceKind::Disguise;
                    r.instance_ = instance;
                    r.object_ = subject;
                    return r;
                }
            }
            meter.fail(Error::MissingInfluence);
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<InfluenceRef, Failure> ReferenceHost::bind_restriction(std::uint64_t instance, ActorId actor) const {
        try {
            Meter meter(limits_);
            for (const auto& d : state_.mechanics.restrictions) {
                meter.spend();
                if (instance && d.instance == instance && d.actor == actor && d.until_tick > state_.world.tick) {
                    InfluenceRef r;
                    r.owner_ = owner_;
                    r.kind_ = InfluenceKind::AttackRestriction;
                    r.instance_ = instance;
                    r.actor_ = actor;
                    return r;
                }
            }
            meter.fail(Error::MissingInfluence);
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    void ReferenceHost::check_life(const State& state, Meter& meter) const {
        const auto check = [&](bool good) {
            if (!good)
                meter.fail(Error::InvalidCheckpoint);
        };
        check(state.life.version == 1 && state.life.instances_issued <= limits_.max_servant_instance);
        for (std::size_t i = 0; i < 4; ++i) {
            meter.spend(2);
            const auto& r = state.life.remains[i];
            const auto& s = state.life.servants[i];
            check(r.id.generation <= limits_.max_generation && (!r.id.generation || r.id.slot == i));
            if (r.present)
                check(i < limits_.remains && r.id.generation && valid(r.position));
            else
                check(!r.spent && r.position == Point{});
            if (!s.instance) {
                check(s == ServantView{} && !state.servant_plans[i]);
                continue;
            }
            check(
                i < limits_.servants && s.instance <= state.life.instances_issued && s.action && s.action <= state.world.actions_issued &&
                s.actor.slot < actor_capacity && s.actor.generation && state.world.actors[s.actor.slot].present &&
                state.world.actors[s.actor.slot].id == s.actor && s.owner.slot < actor_capacity && s.owner.generation &&
                s.owner.generation <= limits_.max_generation && s.owner.generation <= state.world.actors[s.owner.slot].id.generation &&
                s.remains.slot < 4 && state.life.remains[s.remains.slot].present && state.life.remains[s.remains.slot].id == s.remains &&
                state.life.remains[s.remains.slot].spent && s.onset_tick <= state.world.tick && s.until_tick > state.world.tick &&
                s.until_tick <= limits_.max_tick
            );
            const auto& plan = state.servant_plans[i];
            payload_schema(plan, meter);
            const auto& m = plan->meaning();
            check(
                m.entry.kind == EntryKind::Spoken && m.delivery == Delivery::Remains && !m.onset_ticks && m.effects.size() == 1 &&
                m.effects[0].primitive == Primitive::Animate && m.bindings.size() == 1 && m.bindings[0].query == Query::CapturedRemains &&
                m.effects[0].binding == 0 && s.until_tick - s.onset_tick == std::get<Animation>(m.effects[0].payload).duration_ticks
            );
            for (std::size_t j = 0; j < i; ++j) {
                meter.spend();
                const auto& old = state.life.servants[j];
                check(!old.instance || (old.instance != s.instance && old.actor != s.actor && old.remains != s.remains));
            }
        }
    }
}
