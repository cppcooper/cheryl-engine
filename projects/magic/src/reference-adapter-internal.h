#pragma once

#include <magic/runtime.h>
#include <magic/reference-host.h>

namespace Magic::Reference {
    class ContactView final : public Contact::WorldAdapter {
        const Catalog& catalog_;
        const Snapshot& world_;
    public:
        ContactView(const Catalog& catalog, const Snapshot& world) : catalog_(catalog), world_(world) {}
        const Catalog& catalog() const noexcept override { return catalog_; }
        std::expected<Contact::Facts, Contact::Error> inspect(Contact::Request request) const override {
            auto find = [&](Contact::Entity id) -> const ActorView* {
                if (id.domain != 1 || id.key >= actor_capacity || id.generation == 0)
                    return nullptr;
                const auto& a = world_.actors[static_cast<std::size_t>(id.key)];
                return a.present && a.id.generation == id.generation ? &a : nullptr;
            };
            const auto* caster = find(request.caster);
            const auto* target = find(request.recipient);
            if (!caster || !target)
                return std::unexpected(Contact::Error::MissingEntity);
            const auto dx = static_cast<std::int64_t>(caster->data.position.x) - target->data.position.x;
            const auto dy = static_cast<std::int64_t>(caster->data.position.y) - target->data.position.y;
            return Contact::Facts{world_.revision, world_.revision, target->data.health, target->data.max_health,
                caster->data.mana, 10, caster->data.health != 0, caster->data.can_cast,
                target->data.health != 0, target->data.eligible, target->data.willing, dx * dx + dy * dy <= 2500};
        }
        std::expected<std::unique_ptr<Contact::Candidate>, Contact::Error> candidate(const Contact::Change&) override {
            // The retained facade owns its existing full State candidate/adoption.
            return std::unexpected(Contact::Error::Preparation);
        }
        static Contact::Request request(ActorId caster, ActorId target) {
            return {{1, caster.slot, caster.generation}, {1, target.slot, target.generation}};
        }
    };
    inline Error contact_error(Contact::Error error) {
        switch (error) {
            case Contact::Error::NoPlan: return Error::NoPlan;
            case Contact::Error::PlanLimit: return Error::PlanLimit;
            case Contact::Error::MissingEntity: return Error::MissingActor;
            case Contact::Error::Permission: return Error::Permission;
            case Contact::Error::Contact: return Error::Contact;
            case Contact::Error::Resources: return Error::Resources;
            case Contact::Error::WorkLimit: return Error::WorkLimit;
            default: return Error::SchemaMismatch;
        }
    }
}
