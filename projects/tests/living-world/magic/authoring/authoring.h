#pragma once

#include <host/host.h>

#include <array>
#include <string>

namespace CE::LivingWorld {
    enum class DraftState { Accepted, Invalid, Unsupported, Refused };
    struct SpellDraft {
        std::uint64_t id = 0;
        std::string source;
        DraftState state = DraftState::Invalid;
        std::optional<Host::Accepted> accepted;
        std::optional<Host::AcceptanceFailure> failure;
    };
    using SpellDraftHandle = std::shared_ptr<const SpellDraft>;

    class MagicAuthoring final {
        Host& host_;
        Host::Access access_;
        std::array<SpellDraftHandle, 8> drafts_{};
        std::size_t size_ = 0;
        SpellDraftHandle latest_;
    public:
        MagicAuthoring(Host& host, Host::Access access) noexcept;
        std::expected<SpellDraftHandle, Error> edit(std::string_view source) noexcept;
        SpellDraftHandle latest() const noexcept;
        std::expected<Host::CastQuote, Error> quote(EntityRef recipient) const noexcept;
        std::expected<Action, Error> action(EntityRef recipient);
    };
}
