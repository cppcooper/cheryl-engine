#include <host/host.h>
#include <magic/contact/view.h>

#include <algorithm>
#include <atomic>
#include <limits>
#include <stdexcept>
#include <utility>

namespace CE::LivingWorld {
    namespace {
        constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
        Reason reason(::Magic::Contact::Error error) noexcept {
            using ProducerError = ::Magic::Contact::Error;
            switch (error) {
            case ProducerError::Permission: return Reason::Permission;
            case ProducerError::Contact: return Reason::Contact;
            case ProducerError::Resources: return Reason::Resources;
            case ProducerError::MissingEntity: return Reason::Stale;
            case ProducerError::Schema: return Reason::Schema;
            default: return Reason::Preparation;
            }
        }
    }

    std::expected<OwnerFence, Error> Host::allocate_fence() noexcept {
        static std::atomic<std::uint64_t> issued{0};
        auto previous = issued.load(std::memory_order_relaxed);
        do {
            if (previous == maximum)
                return std::unexpected(Error::Exhausted);
        } while (!issued.compare_exchange_weak(previous, previous + 1, std::memory_order_relaxed));
        return OwnerFence{previous + 1, 1};
    }

    Host::Host(::Magic::Catalog catalog, ::Magic::PlanHandle plan, WorldScope scope,
               std::uint32_t cap, std::uint32_t price)
    : catalog_(std::move(catalog)), plan_(std::move(plan)),
      authority_(std::make_shared<const unsigned char>(0)), image_(std::make_unique<Detail::Image>()) {
        const auto initial = binding(catalog_, plan_, 1);
        if (!scope.world || !scope.branch || !initial)
            throw std::invalid_argument("Host requires a scoped compiler-produced Contact Heal plan");
        image_->scope = scope;
        const auto fresh = allocate_fence();
        if (!fresh)
            throw std::overflow_error("Host live identity exhausted");
        image_->fence = *fresh;
        lineage_ = authority_;
        image_->quote.price = price;
        image_->binding = *initial;
        image_->plans[image_->plan_count++] = {*initial, plan_};
        image_->actor_count = 4;
        for (std::size_t index = 0; index < image_->actor_count; ++index) {
            auto& actor = image_->actors[index];
            actor.ref = {scope, index + 1, 1};
            actor.cap = cap;
            actor.observes = index == 2;
        }
    }

    std::optional<PlanBinding> Host::binding(const ::Magic::Catalog& catalog,
                                            const ::Magic::PlanHandle& plan, std::uint64_t id) noexcept {
        PlanBinding result;
        if (!id || !::Magic::Contact::supports(plan) || plan->source().size() > result.source.size() ||
            plan->metrics().retained_bytes > 32768 || catalog.operations.size() != 1 ||
            !catalog.spoken || !catalog.contact ||
            catalog.operations[0].primitive != ::Magic::Primitive::Heal ||
            catalog.operations[0].key != plan->meaning().effects[0].key ||
            catalog.operations[0].key.size() > result.operation.size() ||
            plan->meaning().effects[0].quantity > catalog.operations[0].max_quantity)
            return {};
        result.id = id;
        result.source_size = plan->source().size();
        std::copy(plan->source().begin(), plan->source().end(), result.source.begin());
        const auto& operation = catalog.operations[0];
        result.operation_size = operation.key.size();
        std::copy(operation.key.begin(), operation.key.end(), result.operation.begin());
        result.max_quantity = operation.max_quantity;
        result.max_duration = operation.max_duration_ticks;
        result.max_coordinate = operation.max_coordinate_mm;
        result.appearances = operation.appearances;
        const std::array capabilities{
            catalog.spoken, catalog.engraved, catalog.contact, catalog.ray, catalog.burst,
            catalog.item_event, catalog.together, catalog.annular, catalog.subject
        };
        for (std::size_t index = 0; index < capabilities.size(); ++index)
            if (capabilities[index])
                result.capabilities |= static_cast<std::uint16_t>(1u << index);
        return result;
    }

    const Detail::AcceptedPlan* Host::find_plan(const Detail::Image& image, const PlanBinding& binding) noexcept {
        if (image.plan_count > image.plans.size())
            return nullptr;
        auto content = binding;
        content.schema = 1;
        for (std::size_t index = 0; index < image.plan_count; ++index)
            if (image.plans[index].binding == content)
                return &image.plans[index];
        return nullptr;
    }

    const Detail::AcceptedPlan* Host::resolve(const Access& access, const Accepted& accepted) const noexcept {
        if (!authorized(access) || access.principal_.id != 1 || accepted.owner_ != authority_ ||
            accepted.scope_ != image_->scope)
            return nullptr;
        for (std::size_t index = 0; index < image_->plan_count; ++index)
            if (image_->plans[index].binding.id == accepted.id_)
                return &image_->plans[index];
        return nullptr;
    }

    std::expected<Host::Accepted, Host::AcceptanceFailure>
    Host::accept(const Access& access, std::string_view source) noexcept {
        if (!authorized(access) || access.principal_.id != 1)
            return std::unexpected(AcceptanceFailure{Error::Unauthorized, {}, false});
        if (reserved_)
            return std::unexpected(AcceptanceFailure{Error::Busy, {}, false});
        try {
            auto compiled = ::Magic::compile(source, catalog_);
            if (!compiled)
                return std::unexpected(AcceptanceFailure{Error::InvalidBody, std::move(compiled.error()), false});
            const auto content = binding(catalog_, *compiled, image_->plan_count + 1);
            if (!content)
                return std::unexpected(AcceptanceFailure{Error::InvalidBody, {}, true});
            Accepted accepted;
            accepted.owner_ = authority_;
            accepted.scope_ = image_->scope;
            for (std::size_t index = 0; index < image_->plan_count; ++index) {
                if (image_->plans[index].plan->source() == source) {
                    accepted.id_ = image_->plans[index].binding.id;
                    return accepted;
                }
            }
            if (image_->plan_count == image_->plans.size())
                return std::unexpected(AcceptanceFailure{Error::Capacity, {}, false});
            if (image_->revision > maximum - 2)
                return std::unexpected(AcceptanceFailure{Error::Exhausted, {}, false});
            auto next = std::make_unique<Detail::Image>(*image_);
            next->plans[next->plan_count++] = {*content, std::move(*compiled)};
            ++next->revision;
            accepted.id_ = content->id;
            image_.swap(next);
            return accepted;
        } catch (...) {
            return std::unexpected(AcceptanceFailure{Error::Preparation, {}, false});
        }
    }

    std::expected<::Magic::PlanHandle, Error> Host::inspect(const Access& access, const Accepted& accepted) const noexcept {
        const auto entry = resolve(access, accepted);
        if (!entry)
            return std::unexpected(Error::Unauthorized);
        return entry->plan;
    }

    std::expected<Host::CastQuote, Error>
    Host::quote(const Access& access, const Accepted& accepted, EntityRef recipient) const noexcept {
        const auto entry = resolve(access, accepted);
        if (!entry)
            return std::unexpected(Error::Unauthorized);
        if (!actor_index(recipient))
            return std::unexpected(Error::Missing);
        if (reserved_)
            return std::unexpected(Error::Busy);
        Detail::ContactView view(*image_, catalog_);
        const ::Magic::Contact::Request request{
            Detail::ContactView::entity(access.principal_), Detail::ContactView::entity(recipient)
        };
        try {
            const auto evaluation = ::Magic::Contact::evaluate(entry->plan, request, view);
            if (!evaluation)
                return std::unexpected(Error::InvalidBody);
            CastQuote result;
            result.owner_ = authority_;
            result.body_ = {entry->binding, access.principal_, recipient, image_->quote,
                            reads(access.principal_, recipient), image_->policy_revision};
            result.body_.plan.schema = image_->binding.schema;
            return result;
        } catch (...) {
            return std::unexpected(Error::Preparation);
        }
    }

    std::expected<Action, Error> Host::action(const Access& access, const CastQuote& quote) {
        if (!authorized(access) || access.principal_.id != 1 || quote.owner_ != authority_ ||
            quote.body_.caster != access.principal_)
            return std::unexpected(Error::Unauthorized);
        if (reserved_)
            return std::unexpected(Error::Busy);
        if (!live(quote.body_))
            return std::unexpected(Error::Stale);
        return issue(quote.body_);
    }

    std::expected<Action, Error> Host::action(const Access& access, EntityRef recipient, const Accepted& accepted) {
        const auto quoted = quote(access, accepted, recipient);
        if (!quoted)
            return std::unexpected(quoted.error());
        return action(access, *quoted);
    }

    Host::Reservation::Reservation(Host& host) noexcept : host_(&host) {}
    Host::Reservation::Reservation(Reservation&& other) noexcept : host_(std::exchange(other.host_, nullptr)) {}
    Host::Reservation::~Reservation() noexcept {
        if (host_) {
            host_->publish();
            host_->reserved_ = false;
        }
    }
    void Host::Reservation::finalize() noexcept {
        auto& host = *host_;
        host.queued_ = host.image_->record_count;
        host.staging_->records[*host.queued_].cancelled = false;
        host.publish();
        host.reserved_ = false;
        host_ = nullptr;
    }

    void Host::publish() noexcept {
        static_assert(noexcept(image_.swap(staging_)));
        image_.swap(staging_);
    }

    std::optional<std::size_t> Host::actor_index(EntityRef ref) const noexcept {
        for (std::size_t index = 0; index < image_->actor_count; ++index)
            if (image_->actors[index].ref == ref)
                return index;
        return {};
    }
    bool Host::authorized(const Access& access) const noexcept {
        if (!access.owner_ || access.owner_ != authority_)
            return false;
        const auto index = actor_index(access.principal_);
        return index && image_->actors[*index].access;
    }
    Host::Access Host::grant(EntityRef principal) const {
        Access access;
        if (actor_index(principal)) {
            access.owner_ = authority_;
            access.principal_ = principal;
        }
        return access;
    }
    EntityRef Host::entity(std::uint64_t id) const noexcept {
        for (std::size_t index = 0; index < image_->actor_count; ++index)
            if (image_->actors[index].ref.id == id)
                return image_->actors[index].ref;
        return {image_->scope, 0, 0};
    }
    OwnerFence Host::fence() const noexcept { return image_->fence; }

    std::expected<Actor, Error> Host::actor(const Access& access, EntityRef ref) const noexcept {
        if (!authorized(access) || access.principal_.id != 1)
            return std::unexpected(Error::Unauthorized);
        const auto index = actor_index(ref);
        if (!index)
            return std::unexpected(Error::Missing);
        return image_->actors[*index];
    }

    ReadSet Host::reads(EntityRef caster, EntityRef recipient) const noexcept {
        ReadSet result;
        result.stamps[result.size++] = {image_->scope, 0, image_->policy_revision};
        for (std::size_t index = 0; index < image_->actor_count; ++index) {
            const auto& actor = image_->actors[index];
            if (actor.ref == caster || actor.ref == recipient)
                result.stamps[result.size++] = {image_->scope, actor.ref.id, actor.revision};
        }
        return result;
    }
    bool Host::canonical(const HealBody& body) noexcept {
        if (body.reads.size == 0 || body.reads.size > body.reads.stamps.size())
            return false;
        for (std::size_t index = 0; index < body.reads.size; ++index) {
            const auto& stamp = body.reads.stamps[index];
            if (stamp.scope != body.caster.scope || !stamp.revision ||
                (index && body.reads.stamps[index - 1].owner >= stamp.owner))
                return false;
        }
        for (std::size_t index = body.reads.size; index < body.reads.stamps.size(); ++index)
            if (body.reads.stamps[index] != Stamp{})
                return false;
        return true;
    }
    bool Host::live(const HealBody& body) const noexcept {
        return body.caster.scope == image_->scope && body.recipient.scope == image_->scope &&
               actor_index(body.caster) && actor_index(body.recipient) && find_plan(*image_, body.plan) &&
               body.plan.schema == image_->binding.schema &&
               body.quote == image_->quote && body.intent == image_->policy_revision &&
               body.reads == reads(body.caster, body.recipient);
    }

    std::expected<Action, Error> Host::action(const Access& access, EntityRef recipient) {
        if (!authorized(access) || access.principal_.id != 1)
            return std::unexpected(Error::Unauthorized);
        if (reserved_)
            return std::unexpected(Error::Busy);
        if (!actor_index(recipient))
            return std::unexpected(Error::Missing);
        return issue({image_->binding, access.principal_, recipient, image_->quote,
                      reads(access.principal_, recipient), image_->policy_revision});
    }

    std::expected<Action, Error> Host::issue(const HealBody& body) {
        if (issued_ == maximum || image_->revision > maximum - 2)
            return std::unexpected(Error::Exhausted);
        auto next = std::make_unique<Detail::Image>(*image_);
        ++next->revision;
        image_.swap(next);
        return Action{{image_->scope, body.caster.id, ++issued_}, body};
    }

    std::expected<Receipt, Error> Host::lookup(const Access& access, const Action& action) const noexcept {
        if (!authorized(access) || action.key.issuer != access.principal_.id ||
            action.key.scope != image_->scope)
            return std::unexpected(Error::Unauthorized);
        for (std::size_t index = 0; index < image_->record_count; ++index) {
            const auto& record = image_->records[index];
            if (record.action.key == action.key) {
                if (record.action.body != action.body)
                    return std::unexpected(Error::Conflict);
                if (record.cancelled)
                    return std::unexpected(Error::InvalidBody);
                return record.receipt;
            }
        }
        return std::unexpected(Error::Missing);
    }

    std::expected<Host::Reservation, Error> Host::reserve(const Access& access, const Action& action, OwnerFence fence) {
        if (!authorized(access) || access.principal_.id != 1 || action.key.issuer != access.principal_.id)
            return std::unexpected(Error::Unauthorized);
        if (action.key.scope != image_->scope)
            return std::unexpected(Error::WrongScope);
        if (fence != image_->fence)
            return std::unexpected(Error::StaleFence);
        const auto existing = lookup(access, action);
        if (existing)
            return std::unexpected(Error::InvalidBody);
        if (existing.error() != Error::Missing)
            return std::unexpected(existing.error());
        if (!canonical(action.body) || action.body.caster != access.principal_ ||
            !action.key.sequence || action.key.sequence > issued_)
            return std::unexpected(Error::InvalidBody);
        if (!live(action.body))
            return std::unexpected(Error::Stale);
        if (reserved_ || queued_)
            return std::unexpected(Error::Busy);
        if (image_->record_count == image_->records.size() || image_->work_count > image_->work.size() - 4)
            return std::unexpected(Error::Capacity);
        if (image_->revision > maximum - 2 || image_->tick == maximum)
            return std::unexpected(Error::Exhausted);
        auto prepared = std::make_unique<Detail::Image>(*image_);
        prepared->records[prepared->record_count++] = {action, fence, {}, true};
        ++prepared->revision;
        staging_ = std::move(prepared);
        reserved_ = true;
        return Reservation(*this);
    }

    std::expected<Receipt, Error> Host::queue(const Access& access, const Action& action, OwnerFence fence) {
        const auto existing = lookup(access, action);
        if (existing || existing.error() != Error::Missing)
            return existing;
        auto reservation = reserve(access, action, fence);
        if (!reservation)
            return std::unexpected(reservation.error());
        reservation->finalize();
        return Receipt{};
    }

    std::expected<Receipt, Error> Host::settle(const Access& access, const Action& action) noexcept {
        const auto existing = lookup(access, action);
        if (!existing || existing->state != Disposition::Queued)
            return existing;
        if (reserved_ || !queued_)
            return std::unexpected(Error::Busy);
        const auto index = *queued_;
        if (image_->records[index].action != action)
            return std::unexpected(Error::Busy);
        *staging_ = *image_;
        auto& receipt = staging_->records[index].receipt;
        receipt = {Disposition::Refused, Reason::Stale, 0};
        const auto caster = actor_index(action.body.caster);
        const auto recipient = actor_index(action.body.recipient);
        if (image_->revision == maximum || image_->tick == maximum)
            return std::unexpected(Error::Exhausted);
        if (image_->records[index].fence == image_->fence && live(action.body) && caster && recipient) {
            Detail::ContactView view(*image_, catalog_);
            const ::Magic::Contact::Request request{
                Detail::ContactView::entity(action.body.caster), Detail::ContactView::entity(action.body.recipient)
            };
            try {
                const auto entry = find_plan(*image_, action.body.plan);
                const auto evaluation = ::Magic::Contact::evaluate(
                    entry ? entry->plan : ::Magic::PlanHandle{}, request, view,
                    {32768, 4096, sizeof(::Magic::Contact::Change), 1});
                if (!evaluation) {
                    receipt.reason = reason(evaluation.error());
                } else {
                    const auto current = view.inspect(request);
                    std::array<EntityRef, 4> witnesses{};
                    std::size_t witness_count = 0;
                    bool capacity = true;
                    for (std::size_t actor = 0; actor < image_->actor_count; ++actor) {
                        const auto& witness = image_->actors[actor];
                        if (witness.living && witness.observes) {
                            if (witness_count == witnesses.size()) {
                                capacity = false;
                                break;
                            }
                            witnesses[witness_count++] = witness.ref;
                        }
                    }
                    const auto& change = evaluation->change;
                    if (!current || *current != change.before || !live(action.body)) {
                        receipt.reason = Reason::Stale;
                    } else if (!capacity || witness_count > image_->work.size() - image_->work_count ||
                               image_->actors[*caster].revision == maximum ||
                               image_->actors[*recipient].revision == maximum) {
                        receipt.reason = Reason::Preparation;
                    } else {
                        staging_->actors[*caster].reserve = change.resource;
                        staging_->actors[*recipient].health = change.health;
                        ++staging_->actors[*caster].revision;
                        if (*recipient != *caster)
                            ++staging_->actors[*recipient].revision;
                        receipt = {Disposition::Applied, Reason::None, change.health - change.before.health};
                        staging_->records[index].applied = change;
                        if (receipt.gain) {
                            auto& record = staging_->records[index];
                            record.witnesses = witnesses;
                            record.witness_count = witness_count;
                            record.observed_at = image_->tick + 1;
                            for (std::size_t witness = 0; witness < witness_count; ++witness)
                                staging_->work[staging_->work_count++] = {
                                    action.key, witnesses[witness], action.body.recipient, image_->tick + 1, false
                                };
                        }
                    }
                }
            } catch (...) {
                *staging_ = *image_;
                staging_->records[index].receipt = {Disposition::Refused, Reason::Preparation, 0};
            }
        }
        ++staging_->revision;
        ++staging_->tick;
        const auto result = staging_->records[index].receipt;
        publish();
        queued_.reset();
        return result;
    }

    std::expected<void, Error> Host::edit_actor(Actor actor) {
        if (reserved_)
            return std::unexpected(Error::Busy);
        std::optional<std::size_t> found;
        for (std::size_t index = 0; index < image_->actor_count; ++index)
            if (image_->actors[index].ref.id == actor.ref.id)
                found = index;
        if (!found || actor.ref.scope != image_->scope || !actor.ref.generation ||
            actor.ref.generation < image_->actors[*found].ref.generation)
            return std::unexpected(Error::InvalidBody);
        if (image_->revision > maximum - 2 || image_->actors[*found].revision == maximum)
            return std::unexpected(Error::Exhausted);
        auto next = std::make_unique<Detail::Image>(*image_);
        actor.revision = image_->actors[*found].revision + 1;
        next->actors[*found] = actor;
        ++next->revision;
        image_.swap(next);
        return {};
    }

    std::uint32_t Host::price() const noexcept { return image_->quote.price; }

    std::expected<void, Error> Host::set_price(std::uint32_t price) {
        if (reserved_)
            return std::unexpected(Error::Busy);
        if (image_->revision > maximum - 2 || image_->policy_revision == maximum || image_->quote.revision == maximum)
            return std::unexpected(Error::Exhausted);
        auto next = std::make_unique<Detail::Image>(*image_);
        next->quote.price = price;
        ++next->quote.revision;
        ++next->policy_revision;
        ++next->revision;
        image_.swap(next);
        return {};
    }
    std::expected<void, Error> Host::set_schema(std::uint64_t schema) {
        if (!schema || schema <= image_->binding.schema)
            return std::unexpected(Error::InvalidBody);
        if (reserved_)
            return std::unexpected(Error::Busy);
        if (image_->revision > maximum - 2 || image_->policy_revision == maximum)
            return std::unexpected(Error::Exhausted);
        auto next = std::make_unique<Detail::Image>(*image_);
        next->binding.schema = schema;
        ++next->policy_revision;
        ++next->revision;
        image_.swap(next);
        return {};
    }
}
