#include <magic/contact/view.h>

namespace CE::LivingWorld::Detail {
    ContactView::ContactView(const Image& image, const ::Magic::Catalog& catalog) noexcept
    : image_(image), catalog_(catalog) {}

    const ::Magic::Catalog& ContactView::catalog() const noexcept { return catalog_; }

    ::Magic::Contact::Entity ContactView::entity(EntityRef ref) noexcept {
        return {ref.scope.world, ref.id, ref.generation};
    }

    std::expected<::Magic::Contact::Facts, ::Magic::Contact::Error>
    ContactView::inspect(::Magic::Contact::Request request) const {
        const Actor* caster = nullptr;
        const Actor* recipient = nullptr;
        for (std::size_t index = 0; index < image_.actor_count; ++index) {
            const auto& actor = image_.actors[index];
            if (entity(actor.ref) == request.caster)
                caster = &actor;
            if (entity(actor.ref) == request.recipient)
                recipient = &actor;
        }
        if (!caster || !recipient)
            return std::unexpected(::Magic::Contact::Error::MissingEntity);
        return ::Magic::Contact::Facts{
            image_.revision, image_.binding.schema, recipient->health, recipient->cap,
            caster->reserve, image_.quote.price, caster->living, caster->can_cast,
            recipient->living, recipient->eligible, recipient->willing, recipient->touching
        };
    }

    std::expected<std::unique_ptr<::Magic::Contact::Candidate>, ::Magic::Contact::Error>
    ContactView::candidate(const ::Magic::Contact::Change&) {
        return std::unexpected(::Magic::Contact::Error::Preparation);
    }
}
