#include <magic/reference-host.h>
#include "reference-meter.h"

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
            if (catalog.operations.size() > primitive_count)
                return false;
            for (std::size_t i = 0; i < catalog.operations.size(); ++i) {
                const auto& schema = catalog.operations[i];
                if (schema.key.empty() || schema.key.size() > 64 || schema.max_quantity == 0 || schema.max_quantity > 1'000'000 ||
                    schema.max_duration_ticks > 8'640'000 || schema.appearances > 3 ||
                    static_cast<unsigned>(schema.primitive) >= primitive_count)
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
                   l.max_generation <= c.max_generation && l.items <= c.items && l.touch_receipts <= c.touch_receipts &&
                   l.max_event_nonce <= c.max_event_nonce && l.max_fact_id <= c.max_fact_id && l.objects <= c.objects &&
                   l.disguises <= c.disguises && l.witnesses <= c.witnesses && l.max_instance <= c.max_instance && l.wards <= c.wards &&
                   l.restrictions <= c.restrictions && l.max_mechanic_instance <= c.max_mechanic_instance;
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
        std::uint64_t distance_squared(Point a, Point b) {
            // Validated +/-1e6 positions keep subtraction in int64; multiply/add are checked independently.
            const auto dx = static_cast<std::int64_t>(a.x) - b.x;
            const auto dy = static_cast<std::int64_t>(a.y) - b.y;
            const auto x2 = square(static_cast<std::uint64_t>(dx < 0 ? -dx : dx));
            const auto y2 = square(static_cast<std::uint64_t>(dy < 0 ? -dy : dy));
            if (x2 > std::numeric_limits<std::uint64_t>::max() - y2)
                refuse(Error::InvalidAim);
            return x2 + y2;
        }
        bool within(Point a, Point b, std::uint32_t radius) {
            return distance_squared(a, b) <= square(radius);
        }
    }

    namespace {
        const ItemView& item(const ItemSnapshot& state, ItemId id) {
            if (id.slot >= item_capacity || id.generation == 0 || !state.items[id.slot].present || state.items[id.slot].id != id)
                refuse(Error::MissingItem);
            return state.items[id.slot];
        }
        void eligible(const Snapshot& world, ActorId id) {
            const auto& a = actor(world, id);
            if (!a.data.eligible || !a.data.willing || a.data.health == 0)
                refuse(Error::Contact);
        }
        bool same_catalog(const Catalog& a, const Catalog& b) {
            if (a.spoken != b.spoken || a.engraved != b.engraved || a.contact != b.contact || a.ray != b.ray || a.burst != b.burst ||
                a.item_event != b.item_event || a.together != b.together || a.annular != b.annular || a.subject != b.subject ||
                a.operations.size() != b.operations.size())
                return false;
            for (std::size_t i = 0; i < a.operations.size(); ++i) {
                const auto& x = a.operations[i];
                const auto& y = b.operations[i];
                if (x.key != y.key || x.primitive != y.primitive || x.max_quantity != y.max_quantity ||
                    x.max_duration_ticks != y.max_duration_ticks || x.appearances != y.appearances)
                    return false;
            }
            return true;
        }
    }

    namespace {
        bool form_valid(Appearance form) {
            return form == Appearance::OpenArch || form == Appearance::StoneWall;
        }
        const ObjectView& object(const PerceptionSnapshot& state, ObjectId id) {
            if (id.slot >= object_capacity || !id.generation || !state.objects[id.slot].present || state.objects[id.slot].id != id)
                refuse(Error::MissingObject);
            return state.objects[id.slot];
        }
    }
    std::expected<ObjectId, Failure> ReferenceHost::add_object(ObjectSpec data) {
        try {
            Meter meter(limits_);
            if (!form_valid(data.form) || !point_valid(data.position))
                meter.fail(Error::InvalidObject);
            auto next = state_;
            for (std::size_t i = 0; i < limits_.objects; ++i) {
                meter.spend();
                auto& slot = next.perception.objects[i];
                if (slot.present)
                    continue;
                if (slot.id.generation == limits_.max_generation)
                    meter.fail(Error::GenerationLimit);
                revision(next.world, limits_);
                ++slot.id.generation;
                slot.present = true;
                slot.data = data;
                slot.fact_revision = next.world.revision;
                const auto id = slot.id;
                state_ = std::move(next);
                return id;
            }
            meter.fail(Error::ObjectLimit);
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<void, Failure> ReferenceHost::update_object(ObjectId id, ObjectSpec data) {
        try {
            Meter meter(limits_);
            static_cast<void>(object(state_.perception, id));
            if (!form_valid(data.form) || !point_valid(data.position))
                meter.fail(Error::InvalidObject);
            auto next = state_;
            revision(next.world, limits_);
            next.perception.objects[id.slot].data = data;
            next.perception.objects[id.slot].fact_revision = next.world.revision;
            state_ = std::move(next);
            return {};
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<void, Failure> ReferenceHost::remove_object(ObjectId id) {
        try {
            Meter meter(limits_);
            static_cast<void>(object(state_.perception, id));
            auto next = state_;
            revision(next.world, limits_);
            erase_object(next, id, meter);
            state_ = std::move(next);
            return {};
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<PerceptionSnapshot, Failure> ReferenceHost::inspect_perception() const {
        try {
            Meter meter(limits_);
            meter.spend(156);
            return state_.perception;
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<KnowledgeView, Failure> ReferenceHost::knowledge(ActorId observer) const {
        try {
            Meter meter(limits_);
            static_cast<void>(actor(state_.world, observer));
            KnowledgeView result;
            meter.spend(witness_capacity); // fixed owned output
            for (std::size_t i = 0; i < state_.perception.witness_count; ++i) {
                meter.spend();
                if (state_.perception.witnesses[i].observer == observer)
                    result.witnesses[result.count++] = state_.perception.witnesses[i];
            }
            return result;
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<Perceived, Failure> ReferenceHost::perceive(ActorId observer, ObjectId subject) const {
        try {
            Meter meter(limits_);
            static_cast<void>(actor(state_.world, observer));
            const auto& physical = object(state_.perception, subject);
            Perceived result{observer, subject, physical.data.form, state_.world.tick};
            meter.spend(4); // owned result
            for (const auto& d : state_.perception.disguises) {
                meter.spend();
                if (!d.instance || d.subject != subject || d.audience[observer.slot] != observer)
                    continue;
                bool revealed = false;
                for (std::size_t i = 0; i < state_.perception.witness_count; ++i) {
                    meter.spend();
                    const auto& w = state_.perception.witnesses[i];
                    if (w.observer == observer && w.instance == d.instance)
                        revealed = true;
                }
                if (!revealed)
                    result.form = d.form;
            }
            return result;
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    void ReferenceHost::perceptual(
        const PlanHandle& plan,
        ObjectId subject,
        const std::array<bool, actor_capacity>& audience,
        State& state,
        Meter& meter
    ) const {
        const auto& physical = object(state.perception, subject);
        auto& p = state.perception;
        const auto& effect = plan->meaning().effects[0];
        if (const auto* payload = std::get_if<VisualDisguise>(&effect.payload)) {
            const auto expiry = later(state.world.tick, payload->duration_ticks, limits_);
            if (p.instances_issued == limits_.max_instance)
                meter.fail(Error::InstanceLimit);
            std::optional<std::size_t> free;
            for (std::size_t i = 0; i < disguise_capacity; ++i) {
                meter.spend();
                const auto& d = p.disguises[i];
                if (!d.instance) {
                    if (i < limits_.disguises && !free)
                        free = i;
                    continue;
                }
                for (std::size_t a = 0; a < actor_capacity; ++a) {
                    meter.spend();
                    if (d.subject == subject && audience[a] && d.audience[a] == state.world.actors[a].id)
                        meter.fail(Error::Overlap);
                }
            }
            if (!free)
                meter.fail(Error::InstanceLimit);
            DisguiseView d;
            d.instance = p.instances_issued + 1;
            d.action = meter.action;
            d.subject = subject;
            d.form = payload->form;
            d.onset_tick = state.world.tick;
            d.until_tick = expiry;
            d.plan_bytes = plan->metrics().retained_bytes;
            for (std::size_t a = 0; a < actor_capacity; ++a) {
                meter.spend();
                if (audience[a])
                    d.audience[a] = state.world.actors[a].id;
            }
            p.disguises[*free] = d;
            state.disguise_plans[*free] = plan;
            ++p.instances_issued;
        } else {
            std::array<Witness, actor_capacity> additions{};
            std::size_t count = 0;
            for (std::size_t a = 0; a < actor_capacity; ++a) {
                meter.spend();
                if (!audience[a])
                    continue;
                const auto observer = state.world.actors[a].id;
                std::uint64_t instance = 0;
                for (const auto& d : p.disguises) {
                    meter.spend();
                    if (d.instance && d.subject == subject && d.audience[a] == observer)
                        instance = d.instance;
                }
                if (!instance)
                    meter.fail(Error::NoDisguise);
                for (std::size_t i = 0; i < p.witness_count; ++i) {
                    meter.spend();
                    if (p.witnesses[i].observer == observer && p.witnesses[i].instance == instance)
                        meter.fail(Error::NoDisguise);
                }
                additions[count++] = {
                    observer, subject, physical.data.form, physical.data.passable, state.world.tick, physical.fact_revision, instance
                };
            }
            if (count > limits_.witnesses - p.witness_count)
                meter.fail(Error::WitnessLimit);
            meter.spend(actor_capacity + count); // fixed scratch witnesses and retained writes
            for (std::size_t i = 0; i < count; ++i)
                p.witnesses[p.witness_count++] = additions[i];
        }
    }

    ReferenceHost::ReferenceHost(Catalog catalog, HostLimits limits)
    : catalog_(std::move(catalog)), limits_(limits), owner_(std::make_shared<const std::uint8_t>(0)) {
        for (std::size_t i = 0; i < actor_capacity; ++i)
            state_.world.actors[i].id.slot = static_cast<std::uint32_t>(i);
        for (std::size_t i = 0; i < item_capacity; ++i)
            state_.items.items[i].id.slot = static_cast<std::uint32_t>(i);
        for (std::size_t i = 0; i < object_capacity; ++i)
            state_.perception.objects[i].id.slot = static_cast<std::uint32_t>(i);
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
            auto next = state_;
            revision(next.world, limits_);
            next.world.actors[id.slot].present = false;
            next.world.actors[id.slot].data = {};
            next.world.actors[id.slot].slow = {};
            for (std::size_t i = 0; i < disguise_capacity; ++i) {
                meter.spend();
                auto& d = next.perception.disguises[i];
                if (!d.instance)
                    continue;
                if (d.audience[id.slot] == id)
                    d.audience[id.slot] = {};
                bool any = false;
                for (const auto& member : d.audience) {
                    meter.spend();
                    any |= member.generation != 0;
                }
                if (!any) {
                    d = {};
                    next.disguise_plans[i].reset();
                }
            }
            forget_actor_mechanics(next, id, meter);
            state_ = std::move(next);
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
        if (meaning.delivery != Delivery::Contact && meaning.delivery != Delivery::Burst && meaning.delivery != Delivery::Subject)
            meter.fail(Error::UnsupportedDelivery);
        if (!catalog_.spoken || (meaning.delivery == Delivery::Contact && !catalog_.contact) ||
            (meaning.delivery == Delivery::Burst && !catalog_.burst) || (meaning.delivery == Delivery::Subject && !catalog_.subject) ||
            (meaning.effects.size() > 1 && !catalog_.together))
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
        } else if (meaning.delivery == Delivery::Subject) {
            if (request.contact || request.aim || !request.subject)
                meter.fail(Error::UnsupportedBinding);
        } else if (!request.aim || request.contact || !point_valid(*request.aim))
            meter.fail(Error::InvalidAim);
        for (const auto& binding : meaning.bindings) {
            meter.spend(binding.name.size() + binding.frame.size() + 1);
            if (meaning.delivery == Delivery::Subject) {
                if (binding.query != Query::CapturedObject || binding.anchor_time != BindingTime::Release ||
                    binding.query_time != BindingTime::Activation)
                    meter.fail(Error::UnsupportedBinding);
            } else if (meaning.delivery == Delivery::Contact) {
                if (binding.query != Query::WillingTouchedActor || binding.query_time != BindingTime::Release)
                    meter.fail(Error::UnsupportedBinding);
            } else {
                const bool annular = binding.query == Query::EligibleAnnulus;
                if ((binding.query != Query::EligibleDisk && !annular) || binding.frame != "G0" || !binding.inclusive ||
                    binding.anchor_time != BindingTime::Release || binding.query_time != BindingTime::Activation ||
                    binding.radius_mm == 0 || binding.radius_mm > 1'000'000 ||
                    (annular ? (!catalog_.annular || binding.inner_radius_mm >= binding.radius_mm) : binding.inner_radius_mm != 0))
                    meter.fail(Error::UnsupportedBinding);
            }
        }
        payload_schema(plan, meter);
        const bool perception = meaning.effects[0].primitive == Primitive::Disguise || meaning.effects[0].primitive == Primitive::Discern;
        const bool object_effect = meaning.effects[0].primitive == Primitive::Shatter;
        if (perception || object_effect) {
            if ((perception && meaning.delivery != Delivery::Burst) || (object_effect && meaning.delivery != Delivery::Subject) ||
                !request.subject)
                meter.fail(Error::UnsupportedBinding);
            const auto& id = *request.subject;
            if (id.slot >= object_capacity || !state.perception.objects[id.slot].present || state.perception.objects[id.slot].id != id ||
                id.generation == 0)
                meter.fail(Error::MissingObject);
            meter.spend();
            if (object_effect) {
                bool supported = false;
                for (const auto& schema : catalog_.operations) {
                    meter.spend();
                    if (schema.primitive == Primitive::Shatter && schema.key == meaning.effects[0].key &&
                        (schema.appearances & (1u << static_cast<unsigned>(state.perception.objects[id.slot].data.form))))
                        supported = true;
                }
                if (!supported)
                    meter.fail(Error::SchemaMismatch);
            }
        } else if (request.subject)
            meter.fail(Error::UnsupportedBinding);
        if (paying)
            static_cast<void>(later(state.world.tick, meaning.onset_ticks, limits_));
        const auto cost = static_cast<std::uint32_t>(meaning.effects.size() * 10); // M1 has <=4 effects
        if (paying && caster.data.mana < cost)
            meter.fail(Error::Resources);
    }

    void ReferenceHost::payload_schema(const PlanHandle& plan, Meter& meter) const {
        if (!plan)
            meter.fail(Error::NoPlan);
        if (plan->metrics().retained_bytes > limits_.plan_bytes)
            meter.fail(Error::PlanLimit);
        const auto& meaning = plan->meaning();
        if (meaning.effects.size() > 1 && !catalog_.together)
            meter.fail(Error::SchemaMismatch);
        for (const auto& effect : meaning.effects) {
            const bool perception = effect.primitive == Primitive::Disguise || effect.primitive == Primitive::Discern;
            if (perception && meaning.effects.size() != 1)
                meter.fail(Error::UnsupportedComposition);
            if ((effect.primitive == Primitive::Ward || effect.primitive == Primitive::Restrict ||
                 effect.primitive == Primitive::Relocate || effect.primitive == Primitive::Shatter) &&
                meaning.effects.size() != 1)
                meter.fail(Error::UnsupportedComposition);
            bool supported = false;
            for (const auto& schema : catalog_.operations) {
                meter.spend(effect.key.size() + 1);
                if (effect.key != schema.key || effect.primitive != schema.primitive)
                    continue;
                if (auto relocation = std::get_if<Relocation>(&effect.payload)) {
                    supported = effect.primitive == Primitive::Relocate && effect.quantity == 0 && effect.duration_ticks == 0 &&
                                point_valid(relocation->destination) && meaning.delivery == Delivery::Contact;
                } else if (auto shatter = std::get_if<ObjectShatter>(&effect.payload)) {
                    supported = effect.primitive == Primitive::Shatter && shatter->subject == SubjectRole::CapturedObject &&
                                effect.quantity == 0 && effect.duration_ticks == 0 && meaning.delivery == Delivery::Subject;
                } else if (auto ward = std::get_if<Absorption>(&effect.payload)) {
                    supported = effect.primitive == Primitive::Ward && effect.quantity == 0 && effect.duration_ticks == 0 &&
                                ward->capacity_dp > 0 && ward->capacity_dp <= schema.max_quantity && ward->duration_ticks > 0 &&
                                ward->duration_ticks <= schema.max_duration_ticks;
                } else if (auto restriction = std::get_if<IntentRestriction>(&effect.payload)) {
                    supported = effect.primitive == Primitive::Restrict && effect.quantity == 0 && effect.duration_ticks == 0 &&
                                restriction->intent == Intent::Attack && restriction->duration_ticks > 0 &&
                                restriction->duration_ticks <= schema.max_duration_ticks;
                } else if (auto d = std::get_if<VisualDisguise>(&effect.payload)) {
                    const auto form = static_cast<unsigned>(d->form);
                    supported = effect.primitive == Primitive::Disguise && effect.quantity == 0 && effect.duration_ticks == 0 &&
                                d->subject == SubjectRole::CapturedObject && form < 2 && (schema.appearances & (1u << form)) &&
                                d->duration_ticks > 0 && d->duration_ticks <= schema.max_duration_ticks;
                } else if (auto d = std::get_if<Discernment>(&effect.payload)) {
                    supported = effect.primitive == Primitive::Discern && d->subject == SubjectRole::CapturedObject &&
                                effect.quantity == 0 && effect.duration_ticks == 0;
                } else
                    supported =
                        !perception && effect.quantity > 0 && effect.quantity <= schema.max_quantity &&
                        effect.duration_ticks <= schema.max_duration_ticks &&
                        ((effect.primitive == Primitive::Slow && effect.quantity <= 1000 && effect.duration_ticks > 0) ||
                         ((effect.primitive == Primitive::Heal || effect.primitive == Primitive::Fire) && effect.duration_ticks == 0));
            }
            if (!supported)
                meter.fail(Error::SchemaMismatch);
        }
    }
    void ReferenceHost::inscription(const PlanHandle& plan, Meter& meter) const {
        payload_schema(plan, meter);
        const auto& m = plan->meaning();
        for (const auto& e : m.effects) {
            meter.spend(); // Count the added envelope scan, including legacy inscriptions.
            if (e.primitive == Primitive::Disguise || e.primitive == Primitive::Discern || e.primitive == Primitive::Ward ||
                e.primitive == Primitive::Restrict)
                meter.fail(Error::UnsupportedEntry);
        }
        if (m.entry.kind != EntryKind::Engraved || m.entry.mode != CastingMode::Release || m.entry.trigger != Trigger::ItemTouch ||
            m.entry.charges != 1 || m.onset_ticks != 0)
            meter.fail(Error::UnsupportedEntry);
        if (m.delivery != Delivery::ItemEvent)
            meter.fail(Error::UnsupportedDelivery);
        if (!catalog_.engraved || !catalog_.item_event)
            meter.fail(Error::SchemaMismatch);
        for (const auto& b : m.bindings) {
            meter.spend(b.name.size() + b.frame.size() + 1);
            if (b.query != Query::WillingEventToucher || b.query_time != BindingTime::Activation)
                meter.fail(Error::UnsupportedBinding);
        }
    }

    void ReferenceHost::apply(
        const PlanHandle& plan,
        std::optional<ActorId> recipient,
        std::optional<Point> aim,
        std::optional<ObjectId> subject,
        State& state,
        Meter& meter
    ) const {
        const auto& meaning = plan->meaning();
        std::array<std::array<bool, actor_capacity>, 8> selected{};
        // Resolve every binding against the same pre-activation world before building any writes.
        for (std::size_t b = 0; b < meaning.bindings.size(); ++b) {
            bool any = false;
            const auto& binding = meaning.bindings[b];
            if (binding.query == Query::CapturedObject) {
                meter.visit();
                meter.select();
                if (!subject || subject->slot >= object_capacity || !state.perception.objects[subject->slot].present ||
                    state.perception.objects[subject->slot].id != *subject)
                    meter.fail(Error::MissingObject);
                any = true;
            } else if (binding.query == Query::WillingTouchedActor || binding.query == Query::WillingEventToucher) {
                meter.visit();
                meter.select();
                selected[b][recipient->slot] = true;
                any = true;
            } else {
                const bool annular = binding.query == Query::EligibleAnnulus;
                if (binding.query != Query::EligibleDisk && !annular)
                    meter.fail(Error::UnsupportedBinding);
                std::uint64_t inner2 = 0, outer2 = 0;
                if (annular) {
                    meter.spend(2);
                    inner2 = square(binding.inner_radius_mm);
                    outer2 = square(binding.radius_mm);
                }
                for (std::size_t i = 0; i < actor_capacity; ++i) {
                    meter.visit();
                    if (annular)
                        meter.visit(); // explicit inner comparison/selector charge, including empty slots
                    const auto& slot = state.world.actors[i];
                    if (!slot.present || slot.data.health == 0 || !slot.data.eligible)
                        continue;
                    const auto distance = annular ? distance_squared(slot.data.position, *aim) : 0;
                    const bool inside =
                        annular ? distance > inner2 && distance <= outer2 : within(slot.data.position, *aim, binding.radius_mm);
                    if (inside) {
                        meter.select();
                        selected[b][i] = true;
                        any = true;
                    }
                }
            }
            if (!any)
                meter.fail(Error::EmptySelection);
        }
        if (meaning.effects[0].primitive == Primitive::Shatter) {
            erase_object(state, *subject, meter);
            return;
        }
        if (meaning.effects[0].primitive == Primitive::Relocate) {
            std::optional<ActorId> target;
            for (std::size_t i = 0; i < actor_capacity; ++i) {
                meter.spend();
                if (selected[meaning.effects[0].binding][i]) {
                    if (target)
                        meter.fail(Error::UnsupportedBinding);
                    target = state.world.actors[i].id;
                }
            }
            if (!target)
                meter.fail(Error::EmptySelection);
            relocate(std::get<Relocation>(meaning.effects[0].payload), *target, state, meter);
            return;
        }
        if (meaning.effects[0].primitive == Primitive::Ward || meaning.effects[0].primitive == Primitive::Restrict) {
            mechanical(plan, selected[meaning.effects[0].binding], state, meter);
            return;
        }
        if (meaning.effects[0].primitive == Primitive::Disguise || meaning.effects[0].primitive == Primitive::Discern) {
            perceptual(plan, *subject, selected[meaning.effects[0].binding], state, meter);
            return;
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
                static_cast<void>(damage(data.id, write.quantity, state, meter));
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
                apply(plan, request.contact, request.aim, request.subject, scratch, meter);
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
                next.world.pending[slot] = {true,
                                            id,
                                            due,
                                            input.request.caster,
                                            input.request.aim.value_or(Point{}),
                                            input.plan->metrics().retained_bytes,
                                            input.request.subject};
                next.plans[slot] = input.plan;
                kind = EventKind::Queued;
            } else
                apply(input.plan, input.request.contact, input.request.aim, input.request.subject, next, meter);
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
                for (const auto& d : next.perception.disguises) {
                    meter.spend();
                    if (d.instance && d.until_tick <= target && (!due || d.until_tick < *due))
                        due = d.until_tick;
                }
                const auto mechanics_due = mechanic_due(next, target, meter);
                if (mechanics_due && (!due || *mechanics_due < *due))
                    due = mechanics_due;
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
                for (std::size_t i = 0; i < disguise_capacity; ++i) {
                    meter.spend();
                    if (next.perception.disguises[i].instance && next.perception.disguises[i].until_tick <= *due) {
                        next.perception.disguises[i] = {};
                        next.disguise_plans[i].reset();
                    }
                }
                expire_mechanics(next, *due, meter);
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
                    const Request request{
                        p.caster, std::nullopt,
                        next.plans[*index]->meaning().delivery == Delivery::Burst ? std::optional<Point>(p.aim) : std::nullopt, p.subject
                    };
                    validate(next.plans[*index], request, next, meter, false);
                    apply(next.plans[*index], request.contact, request.aim, request.subject, next, meter);
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

    std::expected<ItemId, Failure> ReferenceHost::install(const PlanHandle& plan, ActorId funder) {
        try {
            Meter meter(limits_);
            inscription(plan, meter);
            const auto& payer = actor(state_.world, funder);
            if (!payer.data.can_cast || payer.data.health == 0)
                meter.fail(Error::Permission);
            const auto cost = static_cast<std::uint32_t>(plan->meaning().effects.size() * 10);
            if (payer.data.mana < cost)
                meter.fail(Error::Resources);
            auto next = state_;
            for (std::size_t i = 0; i < limits_.items; ++i) {
                meter.spend();
                auto& slot = next.items.items[i];
                if (slot.present)
                    continue;
                if (slot.id.generation == limits_.max_generation)
                    meter.fail(Error::GenerationLimit);
                revision(next.world, limits_);
                ++slot.id.generation;
                slot.present = true;
                slot.enabled = true;
                slot.charges = 1;
                slot.prepaid_mana = cost;
                slot.plan_bytes = plan->metrics().retained_bytes;
                next.inscriptions[i] = plan;
                next.world.actors[funder.slot].data.mana -= cost;
                const auto id = slot.id;
                state_ = std::move(next);
                return id;
            }
            meter.fail(Error::ItemLimit);
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<void, Failure> ReferenceHost::enable_item(ItemId id, bool enabled) {
        try {
            Meter meter(limits_);
            static_cast<void>(item(state_.items, id));
            auto next = state_;
            revision(next.world, limits_);
            next.items.items[id.slot].enabled = enabled;
            state_ = std::move(next);
            return {};
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<void, Failure> ReferenceHost::remove_item(ItemId id) {
        try {
            Meter meter(limits_);
            static_cast<void>(item(state_.items, id));
            auto next = state_;
            revision(next.world, limits_);
            next.items.items[id.slot] = {id};
            next.inscriptions[id.slot].reset();
            state_ = std::move(next);
            return {};
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<TouchEvent, Failure> ReferenceHost::admit_touch(std::uint64_t fact, ItemId id, ActorId toucher) {
        try {
            Meter meter(limits_);
            if (fact == 0 || fact > limits_.max_fact_id)
                meter.fail(Error::InvalidEvent);
            static_cast<void>(item(state_.items, id));
            static_cast<void>(actor(state_.world, toucher));
            for (const auto& record : state_.items.ledger) {
                meter.spend();
                if (record.fact == fact)
                    meter.fail(Error::DuplicateFact);
            }
            if (state_.items.events_issued == limits_.touch_receipts)
                meter.fail(Error::EventLimit);
            if (state_.items.events_issued == limits_.max_event_nonce)
                meter.fail(Error::NonceLimit);
            auto next = state_;
            revision(next.world, limits_);
            const auto index = static_cast<std::size_t>(next.items.events_issued);
            const auto nonce = next.items.events_issued + 1;
            next.items.ledger[index] = {fact, nonce, id, toucher, false};
            next.items.events_issued = nonce;
            TouchEvent event(owner_, nonce);
            state_ = std::move(next);
            return event;
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<TouchEvent, Failure> ReferenceHost::resume_touch(std::uint64_t fact) const {
        try {
            Meter meter(limits_);
            if (fact == 0 || fact > limits_.max_fact_id)
                meter.fail(Error::InvalidEvent);
            for (const auto& record : state_.items.ledger) {
                meter.spend();
                if (record.fact != fact)
                    continue;
                if (record.consumed)
                    meter.fail(Error::RepeatedEvent);
                static_cast<void>(item(state_.items, record.item));
                static_cast<void>(actor(state_.world, record.toucher));
                return TouchEvent(owner_, record.nonce);
            }
            meter.fail(Error::MissingFact);
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<ItemObserved, Failure> ReferenceHost::touch(const TouchEvent& event) {
        try {
            Meter meter(limits_);
            if (!event.owner_)
                meter.fail(Error::InvalidEvent);
            if (event.owner_ != owner_)
                meter.fail(Error::ForeignEvent);
            if (event.nonce_ == 0 || event.nonce_ > state_.items.events_issued)
                meter.fail(Error::InvalidEvent);
            const auto index = static_cast<std::size_t>(event.nonce_ - 1);
            const auto& record = state_.items.ledger[index];
            if (record.consumed)
                meter.fail(Error::RepeatedEvent);
            const auto& current = item(state_.items, record.item);
            if (!current.enabled)
                meter.fail(Error::DisabledItem);
            if (current.charges == 0)
                meter.fail(Error::SpentItem);
            inscription(state_.inscriptions[record.item.slot], meter);
            eligible(state_.world, record.toucher);
            auto next = state_;
            revision(next.world, limits_);
            apply(next.inscriptions[record.item.slot], record.toucher, std::nullopt, std::nullopt, next, meter);
            --next.items.items[record.item.slot].charges;
            next.items.ledger[index].consumed = true;
            auto report =
                std::make_shared<const ItemObservation>(ItemObservation{state_.world, next.world, state_.items, next.items, meter.usage});
            static_assert(std::is_nothrow_move_assignable_v<State>);
            state_ = std::move(next);
            return ItemObserved(std::move(report));
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }

    struct Checkpoint::Data {
        ReferenceHost::State state;
        Catalog catalog;
        HostLimits limits;
        std::uint64_t tickets_issued;
    };
    void ReferenceHost::continuation(const State& state, std::uint64_t tickets, Meter& meter) const {
        const auto& w = state.world;
        const auto& it = state.items;
        auto check = [&](bool valid) {
            if (!valid)
                meter.fail(Error::InvalidCheckpoint);
        };
        check(
            w.tick <= limits_.max_tick && w.revision <= limits_.max_revision && w.actions_issued <= limits_.max_action &&
            tickets <= limits_.max_ticket && it.events_issued <= limits_.touch_receipts && it.events_issued <= limits_.max_event_nonce
        );
        std::size_t statuses = 0;
        for (std::size_t i = 0; i < actor_capacity; ++i) {
            meter.spend();
            const auto& a = w.actors[i];
            check(a.id.slot == i && a.id.generation <= limits_.max_generation);
            if (a.present) {
                check(i < limits_.actors && a.id.generation != 0 && spec_valid(a.data));
                if (a.slow.until_tick) {
                    ++statuses;
                    check(
                        a.slow.until_tick > w.tick && a.slow.until_tick <= limits_.max_tick && a.slow.permille > 0 &&
                        a.slow.permille <= 1000
                    );
                } else
                    check(a.slow == Slow{});
            } else
                check(a.data == ActorSpec{} && a.slow == Slow{});
        }
        check(statuses <= limits_.statuses);
        for (std::size_t i = 0; i < pending_capacity; ++i) {
            meter.spend();
            const auto& p = w.pending[i];
            if (!p.present) {
                check(p == PendingView{} && !state.plans[i]);
                continue;
            }
            check(
                i < limits_.pending && p.action > 0 && p.action <= w.actions_issued && p.due_tick > w.tick && p.due_tick <= limits_.max_tick
            );
            check(
                state.plans[i] && state.plans[i]->metrics().retained_bytes == p.plan_bytes && state.plans[i]->meaning().onset_ticks != 0 &&
                (state.plans[i]->meaning().delivery == Delivery::Burst || state.plans[i]->meaning().delivery == Delivery::Subject)
            );
            for (std::size_t j = 0; j < i; ++j) {
                meter.spend();
                check(!w.pending[j].present || w.pending[j].action != p.action);
            }
            check(state.plans[i]->meaning().delivery == Delivery::Burst || p.aim == Point{});
            validate(
                state.plans[i],
                Request{
                    p.caster, std::nullopt,
                    state.plans[i]->meaning().delivery == Delivery::Burst ? std::optional<Point>(p.aim) : std::nullopt, p.subject
                },
                state, meter, false
            );
            for (const auto& effect : state.plans[i]->meaning().effects) {
                meter.spend();
                static_cast<void>(later(p.due_tick, effect_duration_ticks(effect), limits_));
            }
        }
        for (std::size_t i = 0; i < item_capacity; ++i) {
            meter.spend();
            const auto& a = it.items[i];
            check(a.id.slot == i && a.id.generation <= limits_.max_generation);
            if (!a.present) {
                check(a == ItemView{a.id} && !state.inscriptions[i]);
                continue;
            }
            check(i < limits_.items && a.id.generation != 0 && a.charges <= 1 && state.inscriptions[i]);
            inscription(state.inscriptions[i], meter);
            check(
                a.plan_bytes == state.inscriptions[i]->metrics().retained_bytes &&
                a.prepaid_mana == state.inscriptions[i]->meaning().effects.size() * 10
            );
        }
        for (std::size_t i = 0; i < touch_capacity; ++i) {
            meter.spend();
            const auto& r = it.ledger[i];
            if (i >= it.events_issued) {
                check(r == TouchRecord{});
                continue;
            }
            check(
                r.fact > 0 && r.fact <= limits_.max_fact_id && r.nonce == i + 1 && r.item.slot < limits_.items && r.item.generation > 0 &&
                r.item.generation <= it.items[r.item.slot].id.generation && r.toucher.slot < limits_.actors && r.toucher.generation > 0 &&
                r.toucher.generation <= w.actors[r.toucher.slot].id.generation
            );
            for (std::size_t j = 0; j < i; ++j) {
                meter.spend();
                check(it.ledger[j].fact != r.fact);
            }
            // Historical references may be departed/replaced. They remain receipts, never live authority.
        }
        const auto& ps = state.perception;
        check(ps.instances_issued <= limits_.max_instance && ps.witness_count <= limits_.witnesses);
        for (std::size_t i = 0; i < object_capacity; ++i) {
            meter.spend();
            const auto& o = ps.objects[i];
            check(o.id.slot == i && o.id.generation <= limits_.max_generation);
            if (o.present)
                check(
                    i < limits_.objects && o.id.generation && form_valid(o.data.form) && point_valid(o.data.position) && o.fact_revision &&
                    o.fact_revision <= w.revision
                );
            else
                check(o == ObjectView{o.id});
        }
        for (std::size_t i = 0; i < disguise_capacity; ++i) {
            meter.spend();
            const auto& d = ps.disguises[i];
            if (!d.instance) {
                check(d == DisguiseView{} && !state.disguise_plans[i]);
                continue;
            }
            check(
                i < limits_.disguises && d.instance <= ps.instances_issued && d.action && d.action <= w.actions_issued &&
                d.onset_tick <= w.tick && d.until_tick > w.tick && d.until_tick <= limits_.max_tick && form_valid(d.form)
            );
            static_cast<void>(object(ps, d.subject));
            const auto& plan = state.disguise_plans[i];
            check(
                plan && d.plan_bytes == plan->metrics().retained_bytes && plan->meaning().effects.size() == 1 &&
                plan->meaning().entry.kind == EntryKind::Spoken && plan->meaning().delivery == Delivery::Burst
            );
            check(catalog_.spoken && catalog_.burst);
            payload_schema(plan, meter);
            const auto* payload = std::get_if<VisualDisguise>(&plan->meaning().effects[0].payload);
            check(payload && payload->form == d.form && d.until_tick - d.onset_tick == payload->duration_ticks);
            for (const auto& b : plan->meaning().bindings) {
                meter.spend(b.name.size() + b.frame.size() + 1);
                check(
                    (b.query == Query::EligibleDisk || (b.query == Query::EligibleAnnulus && catalog_.annular)) && b.frame == "G0" &&
                    b.inclusive && b.anchor_time == BindingTime::Release && b.query_time == BindingTime::Activation && b.radius_mm > 0 &&
                    b.radius_mm <= 1'000'000 &&
                    (b.query == Query::EligibleAnnulus ? b.inner_radius_mm < b.radius_mm : b.inner_radius_mm == 0)
                );
            }
            bool any = false;
            for (std::size_t a = 0; a < actor_capacity; ++a) {
                meter.spend();
                const auto member = d.audience[a];
                if (!member.generation) {
                    check(member == ActorId{});
                    continue;
                }
                any = true;
                check(member.slot == a);
                static_cast<void>(actor(w, member));
                for (std::size_t j = 0; j < i; ++j) {
                    meter.spend();
                    const auto& prior = ps.disguises[j];
                    check(!prior.instance || prior.instance != d.instance);
                    check(!prior.instance || prior.subject != d.subject || prior.audience[a] != member);
                }
            }
            check(any);
        }
        for (std::size_t i = 0; i < witness_capacity; ++i) {
            meter.spend();
            const auto& v = ps.witnesses[i];
            if (i >= ps.witness_count) {
                check(v == Witness{});
                continue;
            }
            check(
                v.observer.slot < limits_.actors && v.observer.generation &&
                v.observer.generation <= w.actors[v.observer.slot].id.generation && v.subject.slot < limits_.objects &&
                v.subject.generation && v.subject.generation <= ps.objects[v.subject.slot].id.generation && form_valid(v.observed_form) &&
                v.observation_tick <= w.tick && v.fact_revision && v.fact_revision <= w.revision && v.instance &&
                v.instance <= ps.instances_issued
            );
            for (std::size_t j = 0; j < i; ++j) {
                meter.spend();
                check(ps.witnesses[j].observer != v.observer || ps.witnesses[j].instance != v.instance);
            }
        }
        check_mechanics(state, meter);
    }
    std::expected<Checkpoint, Failure> ReferenceHost::checkpoint() const {
        try {
            Meter meter(limits_);
            meter.spend(
                220 + (catalog_.operations.size() > 3 ? (catalog_.operations.size() - 3) * 70 : 0)
            ); // bounded catalog metadata/key copy, in addition to fixed State overhead
            continuation(state_, tickets_issued_, meter);
            return Checkpoint(std::make_shared<const Checkpoint::Data>(Checkpoint::Data{state_, catalog_, limits_, tickets_issued_}));
        } catch (Failure f) {
            return std::unexpected(f);
        }
    }
    std::expected<void, Failure> ReferenceHost::restore(const Checkpoint& checkpoint) {
        try {
            Meter meter(limits_);
            if (!checkpoint.data_)
                meter.fail(Error::InvalidCheckpoint);
            const auto& data = *checkpoint.data_;
            // Catalog is bounded (five keys, at most64 bytes each); charge identity comparison before copying.
            meter.spend(220 + (catalog_.operations.size() > 3 ? (catalog_.operations.size() - 3) * 70 : 0));
            if (data.limits != limits_ || !same_catalog(data.catalog, catalog_))
                meter.fail(Error::IncompatibleCheckpoint);
            continuation(data.state, data.tickets_issued, meter);
            auto next = data.state;
            auto incarnation = std::make_shared<const std::uint8_t>(0);
            state_ = std::move(next);
            owner_ = std::move(incarnation);
            tickets_issued_ = data.tickets_issued;
            prepared_.reset();
            return {};
        } catch (Failure f) {
            return std::unexpected(f);
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
            MAGIC_ERROR(ItemLimit);
            MAGIC_ERROR(MissingItem);
            MAGIC_ERROR(DisabledItem);
            MAGIC_ERROR(SpentItem);
            MAGIC_ERROR(InvalidEvent);
            MAGIC_ERROR(ForeignEvent);
            MAGIC_ERROR(RepeatedEvent);
            MAGIC_ERROR(DuplicateFact);
            MAGIC_ERROR(MissingFact);
            MAGIC_ERROR(EventLimit);
            MAGIC_ERROR(NonceLimit);
            MAGIC_ERROR(InvalidCheckpoint);
            MAGIC_ERROR(IncompatibleCheckpoint);
            MAGIC_ERROR(InvalidObject);
            MAGIC_ERROR(ObjectLimit);
            MAGIC_ERROR(MissingObject);
            MAGIC_ERROR(InstanceLimit);
            MAGIC_ERROR(WitnessLimit);
            MAGIC_ERROR(NoDisguise);
            MAGIC_ERROR(UnsupportedComposition);
            MAGIC_ERROR(ActiveWard);
            MAGIC_ERROR(RestrictedIntent);
            MAGIC_ERROR(MechanicLimit);
            MAGIC_ERROR(InvalidIntent);
            MAGIC_ERROR(DestinationBlocked);
        }
#undef MAGIC_ERROR
        return "InvalidError";
    }
}
