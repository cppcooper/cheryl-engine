#include <magic/reference-host.h>
#include "reference-meter.h"

namespace Magic::Reference {
    namespace {
        const ActorView& listener_actor(const Snapshot& world, ActorId id) {
            if (id.slot >= actor_capacity || !id.generation || !world.actors[id.slot].present || world.actors[id.slot].id != id)
                throw Failure{Error::MissingActor};
            return world.actors[id.slot];
        }
        bool point_valid(Point p) {
            return p.x >= -1'000'000 && p.x <= 1'000'000 && p.y >= -1'000'000 && p.y <= 1'000'000;
        }
    }
    std::expected<SoundSnapshot, Failure> ReferenceHost::sounds() const {
        try {
            Meter meter(limits_);
            meter.spend(sound_capacity);
            return state_.sounds;
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<ListenerRef, Failure> ReferenceHost::bind_listener(ActorId listener) const {
        try {
            Meter meter(limits_);
            meter.spend();
            if (!listener_actor(state_.world, listener).data.health)
                meter.fail(Error::Permission);
            ListenerRef ref;
            ref.owner_ = owner_;
            ref.listener_ = listener;
            return ref;
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<Hearing, Failure> ReferenceHost::hear(const ListenerRef& ref) const {
        try {
            Meter meter(limits_);
            if (!ref.owner_)
                meter.fail(Error::InvalidCapability);
            if (ref.owner_ != owner_)
                meter.fail(Error::ForeignCapability);
            meter.spend();
            if (!listener_actor(state_.world, ref.listener_).data.health)
                meter.fail(Error::Permission);
            Hearing heard{ref.listener_, state_.world.tick, {}, 0};
            std::array<std::uint64_t, sound_capacity> identities{};
            meter.spend(sound_capacity * 2); // Owned output and bounded ordering scratch.
            for (const auto& sound : state_.sounds.influences) {
                meter.spend();
                if (!sound.instance || sound.listener != ref.listener_ || sound.until_tick <= state_.world.tick)
                    continue;
                auto at = heard.count;
                while (at && identities[at - 1] > sound.instance) {
                    meter.spend();
                    identities[at] = identities[at - 1];
                    heard.sounds[at] = heard.sounds[at - 1];
                    --at;
                }
                identities[at] = sound.instance;
                heard.sounds[at] = {sound.kind, sound.point};
                ++heard.count;
            }
            return heard;
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    Point ReferenceHost::sound_point(const Effect& effect, Point anchor, Meter& meter) const {
        const auto& sound = std::get<DecoySound>(effect.payload);
        meter.spend(2);
        const auto x = static_cast<std::int64_t>(anchor.x) + sound.offset.x;
        const auto y = static_cast<std::int64_t>(anchor.y) + sound.offset.y;
        if (x < -1'000'000 || x > 1'000'000 || y < -1'000'000 || y > 1'000'000)
            meter.fail(Error::InvalidAim);
        for (const auto& schema : catalog_.operations) {
            meter.spend();
            if (schema.primitive != Primitive::Sound || schema.key != effect.key)
                continue;
            const auto bound = static_cast<std::int64_t>(schema.max_coordinate_mm);
            if (x < -bound || x > bound || y < -bound || y > bound)
                meter.fail(Error::SchemaMismatch);
            return {static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
        }
        meter.fail(Error::SchemaMismatch);
    }
    void ReferenceHost::add_sound(
        const PlanHandle& plan,
        std::size_t effect,
        ActorId listener,
        ActivationSource source,
        State& state,
        Meter& meter
    ) const {
        const auto& e = plan->meaning().effects[effect];
        const auto& payload = std::get<DecoySound>(e.payload);
        if (payload.duration_ticks > limits_.max_tick - state.world.tick)
            meter.fail(Error::TimeLimit);
        if (state.sounds.instances_issued == limits_.max_sound_instance)
            meter.fail(Error::InstanceLimit);
        const auto anchor = listener_actor(state.world, listener).data.position;
        const auto point = sound_point(e, anchor, meter);
        std::array<bool, actor_capacity> listeners{};
        std::optional<std::size_t> free;
        for (std::size_t i = 0; i < sound_capacity; ++i) {
            meter.spend();
            const auto& old = state.sounds.influences[i];
            if (old.instance)
                listeners[old.listener.slot] = true;
            else if (!free && i < limits_.sounds)
                free = i;
        }
        if (!free)
            meter.fail(Error::SoundLimit);
        listeners[listener.slot] = true;
        std::size_t count = 0;
        for (bool active : listeners) {
            meter.spend();
            count += active;
        }
        if (count > limits_.sound_listeners)
            meter.fail(Error::SoundLimit);
        state.sounds.influences[*free] = {
            ++state.sounds.instances_issued,
            listener,
            source,
            effect,
            payload.kind,
            anchor,
            point,
            state.world.tick,
            state.world.tick + payload.duration_ticks,
            plan->metrics().retained_bytes
        };
        state.sound_plans[*free] = plan;
    }
    void ReferenceHost::check_sounds(const State& state, Meter& meter) const {
        const auto check = [&](bool valid) {
            if (!valid)
                meter.fail(Error::InvalidCheckpoint);
        };
        check(state.sounds.version == 1 && state.sounds.instances_issued <= limits_.max_sound_instance);
        std::array<bool, actor_capacity> listeners{};
        for (std::size_t i = 0; i < sound_capacity; ++i) {
            meter.spend();
            const auto& s = state.sounds.influences[i];
            const auto& plan = state.sound_plans[i];
            if (!s.instance) {
                check(s == SoundInfluence{} && !plan);
                continue;
            }
            check(
                i < limits_.sounds && s.instance <= state.sounds.instances_issued && s.listener.slot < limits_.actors &&
                s.listener.generation && state.world.actors[s.listener.slot].present &&
                state.world.actors[s.listener.slot].id == s.listener && s.onset_tick <= state.world.tick &&
                s.until_tick > state.world.tick && s.until_tick <= limits_.max_tick && plan && point_valid(s.captured_anchor) &&
                point_valid(s.point)
            );
            listeners[s.listener.slot] = true;
            payload_schema(plan, meter);
            check(s.plan_bytes == plan->metrics().retained_bytes && s.effect_index < plan->meaning().effects.size());
            const auto& e = plan->meaning().effects[s.effect_index];
            const auto* payload = std::get_if<DecoySound>(&e.payload);
            check(e.primitive == Primitive::Sound && payload && e.binding < plan->meaning().bindings.size());
            check(
                s.kind == payload->kind && s.until_tick - s.onset_tick == payload->duration_ticks &&
                s.point == sound_point(e, s.captured_anchor, meter)
            );
            const auto& origin = s.source;
            if (origin.item) {
                check(
                    plan->meaning().entry.kind == EntryKind::Engraved && origin.caster == ActorId{} && !origin.action &&
                    origin.item->slot < limits_.items && origin.item->generation &&
                    origin.item->generation <= state.items.items[origin.item->slot].id.generation && origin.fact
                );
                bool found = false;
                for (const auto& fact : state.items.ledger) {
                    meter.spend();
                    found |= fact.fact == origin.fact && fact.item == *origin.item && fact.toucher == s.listener && fact.consumed;
                }
                check(found);
                inscription(plan, meter);
            } else {
                check(
                    plan->meaning().entry.kind == EntryKind::Spoken && origin.fact == 0 && origin.action &&
                    origin.action <= state.world.actions_issued && origin.caster.slot < limits_.actors && origin.caster.generation &&
                    origin.caster.generation <= state.world.actors[origin.caster.slot].id.generation && catalog_.spoken &&
                    ((plan->meaning().delivery == Delivery::Contact && catalog_.contact) ||
                     (plan->meaning().delivery == Delivery::Burst && catalog_.burst))
                );
            }
            for (std::size_t j = 0; j < i; ++j) {
                meter.spend();
                const auto& old = state.sounds.influences[j];
                check(
                    !old.instance || (old.instance != s.instance &&
                                      (old.source != origin || old.listener != s.listener || old.effect_index != s.effect_index))
                );
            }
        }
        std::size_t count = 0;
        for (bool active : listeners) {
            meter.spend();
            count += active;
        }
        check(count <= limits_.sound_listeners);
    }
}
