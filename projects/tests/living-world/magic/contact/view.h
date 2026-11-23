#pragma once

#include <host/image.h>
#include <magic/runtime.h>

namespace CE::LivingWorld::Detail {
    class ContactView final : public ::Magic::Contact::WorldAdapter {
        const Image& image_;
        const ::Magic::Catalog& catalog_;

    public:
        ContactView(const Image& image, const ::Magic::Catalog& catalog) noexcept;
        const ::Magic::Catalog& catalog() const noexcept override;
        std::expected<::Magic::Contact::Facts, ::Magic::Contact::Error>
        inspect(::Magic::Contact::Request request) const override;
        std::expected<std::unique_ptr<::Magic::Contact::Candidate>, ::Magic::Contact::Error>
        candidate(const ::Magic::Contact::Change&) override;
        static ::Magic::Contact::Entity entity(EntityRef ref) noexcept;
    };
}
