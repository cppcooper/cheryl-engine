#include <recovery/paired.h>

#include <stdexcept>
#include <utility>

namespace CE::LivingWorld {
    std::expected<void, Error> Recovery::validate(const PairedCut::Data& saved) noexcept {
        const auto& image = saved.image;
        if (saved.schema != 2 || !saved.world_cut || saved.world_cut != saved.scene_cut || !saved.lineage ||
            !image.scope.world || !image.scope.branch || saved.scene_scope != image.scope ||
            saved.baseline != image.revision || !image.revision || !image.policy_revision ||
            !image.fence.instance || !image.fence.epoch || !image.binding.id || !image.binding.revision ||
            !image.binding.catalog || !image.binding.schema || !image.quote.id || !image.quote.revision ||
            image.quote.resource != 1 || image.binding.source_size > image.binding.source.size() ||
            image.binding.operation_size > image.binding.operation.size())
            return std::unexpected(Error::InvalidBody);
        if (!image.actor_count || image.actor_count > image.actors.size() ||
            image.record_count > image.records.size() || image.work_count > image.work.size() ||
            image.acquired_count > image.acquired.size() || !image.plan_count ||
            image.plan_count > image.plans.size())
            return std::unexpected(Error::Capacity);
        for (std::size_t index = 0; index < image.plan_count; ++index) {
            const auto& entry = image.plans[index];
            const auto expected = Host::binding(saved.catalog, entry.plan, index + 1);
            if (!expected || entry.binding != *expected)
                return std::unexpected(Error::InvalidBody);
        }
        auto default_binding = image.binding;
        default_binding.schema = 1;
        if (default_binding != image.plans[0].binding || saved.plan != image.plans[0].plan)
            return std::unexpected(Error::InvalidBody);
        const auto actor_exists = [&image](EntityRef ref) noexcept {
            for (std::size_t index = 0; index < image.actor_count; ++index)
                if (image.actors[index].ref == ref)
                    return true;
            return false;
        };
        for (std::size_t index = 0; index < image.actor_count; ++index) {
            const auto& actor = image.actors[index];
            if (actor.ref.scope != image.scope || !actor.ref.id || !actor.ref.generation || !actor.revision ||
                (index && image.actors[index - 1].ref.id >= actor.ref.id))
                return std::unexpected(Error::InvalidBody);
        }
        const auto valid_action = [&](const Action& action) noexcept {
            const auto& body = action.body;
            if (action.key.scope != image.scope || action.key.issuer != 1 || !action.key.sequence ||
                action.key.sequence > saved.issued || body.caster.id != action.key.issuer ||
                !actor_exists(body.caster) || !actor_exists(body.recipient) || !Host::canonical(body) ||
                !Host::find_plan(image, body.plan) || !body.plan.schema || body.plan.schema > image.binding.schema ||
                !body.intent || body.intent > image.policy_revision || body.quote.id != image.quote.id ||
                !body.quote.revision || body.quote.revision > image.quote.revision || body.quote.resource != 1 ||
                (body.quote.revision == image.quote.revision && body.quote.price != image.quote.price))
                return false;
            std::size_t stamp_index = 0;
            if (body.reads.stamps[stamp_index].owner != 0 ||
                body.reads.stamps[stamp_index++].revision != body.intent)
                return false;
            for (std::size_t index = 0; index < image.actor_count; ++index) {
                const auto& actor = image.actors[index];
                if (actor.ref == body.caster || actor.ref == body.recipient) {
                    if (stamp_index == body.reads.size || body.reads.stamps[stamp_index].owner != actor.ref.id ||
                        body.reads.stamps[stamp_index].revision > actor.revision)
                        return false;
                    ++stamp_index;
                }
            }
            return stamp_index == body.reads.size;
        };
        for (std::size_t index = 0; index < image.record_count; ++index) {
            const auto& record = image.records[index];
            if (!valid_action(record.action) || !record.fence.instance || !record.fence.epoch)
                return std::unexpected(Error::InvalidBody);
            for (std::size_t previous = 0; previous < index; ++previous)
                if (image.records[previous].action.key == record.action.key)
                    return std::unexpected(Error::Conflict);
            if (record.cancelled) {
                if (record.receipt != Receipt{} || record.applied)
                    return std::unexpected(Error::InvalidBody);
            } else if (record.receipt.state == Disposition::Queued) {
                return std::unexpected(Error::Busy);
            } else if (record.receipt.state == Disposition::Applied) {
                const auto entry = Host::find_plan(image, record.action.body.plan);
                if (record.receipt.reason != Reason::None || !entry || !record.applied)
                    return std::unexpected(Error::InvalidBody);
                const auto& change = *record.applied;
                const auto& before = change.before;
                const auto& body = record.action.body;
                if (change.request != ::Magic::Contact::Request{
                        {body.caster.scope.world, body.caster.id, body.caster.generation},
                        {body.recipient.scope.world, body.recipient.id, body.recipient.generation}} ||
                    !before.revision || before.revision > image.revision ||
                    before.schema_revision != body.plan.schema || before.price != body.quote.price ||
                    !before.caster_living || !before.can_cast || !before.recipient_living ||
                    !before.eligible || !before.willing || !before.touching ||
                    !before.health_cap || before.health > before.health_cap || before.resource < before.price ||
                    change.health != ::Magic::Contact::healed(before.health, before.health_cap,
                                                              entry->plan->meaning().effects[0].quantity) ||
                    change.resource != before.resource - before.price ||
                    record.receipt.gain != change.health - before.health)
                    return std::unexpected(Error::InvalidBody);
            } else if (record.receipt.state == Disposition::Refused) {
                if (record.applied || record.receipt.gain || record.receipt.reason < Reason::Stale ||
                    record.receipt.reason > Reason::Preparation)
                    return std::unexpected(Error::InvalidBody);
            } else {
                return std::unexpected(Error::InvalidBody);
            }
            if (record.witness_count > record.witnesses.size())
                return std::unexpected(Error::Capacity);
            const bool incident = !record.cancelled && record.receipt.state == Disposition::Applied &&
                                  record.receipt.gain != 0;
            if (incident ? (!record.observed_at || record.observed_at > image.tick) :
                           (record.witness_count != 0 || record.observed_at != 0))
                return std::unexpected(Error::InvalidBody);
            for (std::size_t witness = 0; witness < record.witness_count; ++witness) {
                if (!actor_exists(record.witnesses[witness]))
                    return std::unexpected(Error::InvalidBody);
                for (std::size_t previous = 0; previous < witness; ++previous)
                    if (record.witnesses[previous] == record.witnesses[witness])
                        return std::unexpected(Error::Conflict);
                std::size_t matching = 0;
                for (std::size_t work = 0; work < image.work_count; ++work)
                    if (image.work[work].cause == record.action.key &&
                        image.work[work].witness == record.witnesses[witness])
                        ++matching;
                if (matching != 1)
                    return std::unexpected(Error::InvalidBody);
            }
        }
        for (std::size_t index = 0; index < image.work_count; ++index) {
            const auto& work = image.work[index];
            if (!actor_exists(work.witness) || !actor_exists(work.subject) ||
                !work.observed_at || work.observed_at > image.tick)
                return std::unexpected(Error::InvalidBody);
            const Detail::Record* cause = nullptr;
            for (std::size_t record = 0; record < image.record_count; ++record)
                if (image.records[record].action.key == work.cause)
                    cause = &image.records[record];
            if (!cause || cause->cancelled || cause->receipt.state != Disposition::Applied ||
                !cause->receipt.gain || cause->action.body.recipient != work.subject ||
                cause->observed_at != work.observed_at)
                return std::unexpected(Error::InvalidBody);
            bool eligible = false;
            for (std::size_t witness = 0; witness < cause->witness_count; ++witness)
                if (cause->witnesses[witness] == work.witness)
                    eligible = true;
            if (!eligible)
                return std::unexpected(Error::InvalidBody);
            std::size_t recipients = 0;
            for (std::size_t previous = 0; previous < image.work_count; ++previous) {
                const auto& other = image.work[previous];
                if (other.cause == work.cause) {
                    ++recipients;
                    if (previous != index && (other.witness == work.witness || other.subject != work.subject ||
                                              other.observed_at != work.observed_at))
                        return std::unexpected(Error::Conflict);
                }
            }
            if (recipients > 4)
                return std::unexpected(Error::Capacity);
            std::size_t acquisitions = 0;
            for (std::size_t acquired = 0; acquired < image.acquired_count; ++acquired) {
                const auto& entry = image.acquired[acquired];
                if (entry.cause == work.cause && entry.witness == work.witness) {
                    if (entry.subject != work.subject || entry.observed_at != work.observed_at)
                        return std::unexpected(Error::InvalidBody);
                    ++acquisitions;
                }
            }
            if (acquisitions != (work.consumed ? 1u : 0u))
                return std::unexpected(Error::InvalidBody);
        }
        for (std::size_t index = 0; index < image.acquired_count; ++index) {
            const auto& entry = image.acquired[index];
            if (!actor_exists(entry.witness) || !actor_exists(entry.subject) ||
                entry.acquired_at <= entry.observed_at || entry.acquired_at > image.tick)
                return std::unexpected(Error::InvalidBody);
            std::uint64_t local_id = 1;
            for (std::size_t previous = 0; previous < index; ++previous) {
                const auto& other = image.acquired[previous];
                if (other.witness == entry.witness) {
                    ++local_id;
                    if (other.cause == entry.cause)
                        return std::unexpected(Error::Conflict);
                }
            }
            if (entry.local_id != local_id)
                return std::unexpected(Error::InvalidBody);
            bool found = false;
            for (std::size_t work = 0; work < image.work_count; ++work)
                if (image.work[work].cause == entry.cause && image.work[work].witness == entry.witness &&
                    image.work[work].consumed)
                    found = true;
            if (!found)
                return std::unexpected(Error::InvalidBody);
        }
        if (!saved.plan || !saved.initial || !saved.helped || !saved.refused ||
            saved.initial->id().value != "living-help" || saved.initial->revision() != 1 ||
            saved.helped->id().value != "living-helped" || saved.helped->revision() != 1 ||
            saved.refused->id().value != "living-refused" || saved.refused->revision() != 1 ||
            saved.principal.id != 1 || !actor_exists(saved.principal) || !valid_action(saved.action) ||
            saved.action.body.caster != saved.principal)
            return std::unexpected(Error::InvalidBody);
        const Detail::Record* dependency = nullptr;
        for (std::size_t index = 0; index < image.record_count; ++index)
            if (image.records[index].action.key == saved.action.key)
                dependency = &image.records[index];
        if (saved.state == SceneState::Offered) {
            if (dependency || saved.navigation.node.value != "offer" || saved.navigation.step != 0)
                return std::unexpected(Error::InvalidBody);
        } else {
            if (!dependency || dependency->cancelled || dependency->action != saved.action)
                return std::unexpected(Error::InvalidBody);
            switch (saved.state) {
            case SceneState::Waiting:
                if (saved.navigation.node.value != "waiting" || saved.navigation.step != 1)
                    return std::unexpected(Error::Navigation);
                break;
            case SceneState::Helped:
            case SceneState::Refused:
                if ((saved.state == SceneState::Helped) != (dependency->receipt.state == Disposition::Applied) ||
                    saved.navigation.node.value != "answer" || saved.navigation.step != 0)
                    return std::unexpected(Error::Navigation);
                break;
            default: return std::unexpected(Error::InvalidBody);
            }
        }
        const bool initial = saved.state == SceneState::Offered || saved.state == SceneState::Waiting;
        const auto& definition = initial ? saved.initial :
            (saved.state == SceneState::Helped ? saved.helped : saved.refused);
        if (saved.navigation.schema != Dialogue::Snapshot::current_schema ||
            saved.navigation.definition != definition->id() ||
            saved.navigation.definition_revision != definition->revision() ||
            saved.navigation.session.value != (initial ? "living-scene" : "living-answer"))
            return std::unexpected(Error::Navigation);
        return {};
    }

    std::expected<PairedCut, Error> Recovery::capture(Host& host, Scene& scene) noexcept {
        if (&scene.host_ != &host)
            return std::unexpected(Error::WrongScope);
        if (host.reserved_ || host.queued_ || host.presenting_ || scene.presenting_)
            return std::unexpected(Error::Busy);
        if (!host.authorized(scene.access_))
            return std::unexpected(Error::Unauthorized);
        try {
            auto saved = std::make_unique<PairedCut::Data>();
            saved->image = *host.image_;
            saved->baseline = host.image_->revision;
            saved->issued = host.issued_;
            saved->scene_scope = host.image_->scope;
            saved->world_cut = std::make_shared<const unsigned char>(0);
            saved->scene_cut = saved->world_cut;
            saved->lineage = host.lineage_;
            saved->catalog = host.catalog_;
            saved->plan = host.plan_;
            saved->initial = scene.initial_;
            saved->helped = scene.helped_;
            saved->refused = scene.refused_;
            const auto navigation = scene.presentation_->session.snapshot();
            if (!navigation)
                return std::unexpected(Error::Navigation);
            saved->navigation = *navigation;
            saved->principal = scene.access_.principal_;
            saved->action = scene.action_;
            saved->state = scene.state_;
            if (const auto valid = validate(*saved); !valid)
                return std::unexpected(valid.error());
            return PairedCut(std::move(saved));
        } catch (...) {
            return std::unexpected(Error::Preparation);
        }
    }

    std::expected<RecoveredPair, Error> Recovery::restore(
        const PairedCut& cut, const Host& current, const Host::Access& authority) noexcept {
        if (!current.authorized(authority) || authority.principal_.id != 1)
            return std::unexpected(Error::Unauthorized);
        if (!cut.data_)
            return std::unexpected(Error::InvalidBody);
        const auto& saved = *cut.data_;
        if (saved.lineage != current.lineage_ || saved.image.scope != current.image_->scope)
            return std::unexpected(Error::WrongScope);
        if (current.reserved_ || current.queued_ || current.presenting_)
            return std::unexpected(Error::Busy);
        if (const auto valid = validate(saved); !valid)
            return std::unexpected(valid.error());
        if (saved.image.binding != current.image_->binding)
            return std::unexpected(Error::Stale);
        for (std::size_t index = 0; index < saved.image.plan_count; ++index) {
            const auto& entry = saved.image.plans[index];
            const auto live = Host::find_plan(*current.image_, entry.binding);
            if (!live || live->plan != entry.plan)
                return std::unexpected(Error::Stale);
        }
        try {
            auto host = std::make_unique<Host>(saved.catalog, saved.plan, saved.image.scope);
            auto expected_binding = host->image_->binding;
            expected_binding.schema = saved.image.binding.schema;
            if (expected_binding != saved.image.binding)
                return std::unexpected(Error::InvalidBody);
            const auto fresh_fence = host->fence();
            *host->image_ = saved.image;
            host->image_->fence = fresh_fence;
            host->issued_ = saved.issued;
            host->lineage_ = saved.lineage;
            for (std::size_t index = 0; index < host->image_->actor_count; ++index) {
                auto& actor = host->image_->actors[index];
                const auto live = current.actor_index(actor.ref);
                if (!live)
                    return std::unexpected(Error::Stale);
                actor.access = current.image_->actors[*live].access;
            }
            host->staging_ = std::make_unique<Detail::Image>(*host->image_);
            auto scene = std::unique_ptr<Scene>(new Scene(*host, host->grant(saved.principal), Scene::Restored{}));
            scene->initial_ = saved.initial;
            scene->helped_ = saved.helped;
            scene->refused_ = saved.refused;
            scene->action_ = saved.action;
            scene->state_ = saved.state;
            const bool initial = saved.state == SceneState::Offered || saved.state == SceneState::Waiting;
            auto session = Dialogue::Session::resume(initial ? saved.initial :
                (saved.state == SceneState::Helped ? saved.helped : saved.refused), saved.navigation);
            if (!session)
                return std::unexpected(Error::Navigation);
            const Dialogue::ConditionHandler condition = [](const Dialogue::ConditionKey& key) noexcept {
                return key.value == "may-help";
            };
            auto view = session->view(condition);
            if (!view)
                return std::unexpected(Error::Navigation);
            if (view->text.size() > 4096 || (saved.state == SceneState::Offered &&
                (view->choices.size() != 1 || view->text.size() + view->choices[0].text.size() > 4096)))
                return std::unexpected(Error::Capacity);
            if (saved.state == SceneState::Offered)
                scene->selection_ = std::move(view->choices[0].selection);
            scene->presentation_ = std::make_unique<Scene::Presentation>(
                Scene::Presentation{std::move(*session), std::move(view->text)});
            return RecoveredPair(std::move(host), std::move(scene));
        } catch (const std::overflow_error&) {
            return std::unexpected(Error::Exhausted);
        } catch (...) {
            return std::unexpected(Error::Preparation);
        }
    }
}
