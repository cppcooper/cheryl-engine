#include <magic/authoring/authoring.h>

#include <utility>

namespace CE::LivingWorld {
    MagicAuthoring::MagicAuthoring(Host& host, Host::Access access) noexcept
    : host_(host), access_(std::move(access)) {}

    std::expected<SpellDraftHandle, Error> MagicAuthoring::edit(std::string_view source) noexcept {
        latest_.reset();
        if (size_ == drafts_.size() || source.size() > 8192)
            return std::unexpected(Error::Capacity);
        try {
            auto draft = std::make_shared<SpellDraft>();
            draft->id = size_ + 1;
            draft->source.assign(source);
            auto accepted = host_.accept(access_, draft->source);
            if (accepted) {
                draft->state = DraftState::Accepted;
                draft->accepted = std::move(*accepted);
            } else {
                draft->state = accepted.error().diagnostic ? DraftState::Invalid :
                    (accepted.error().unsupported ? DraftState::Unsupported : DraftState::Refused);
                draft->failure = std::move(accepted.error());
            }
            drafts_[size_++] = draft;
            latest_ = std::move(draft);
            return latest_;
        } catch (...) {
            return std::unexpected(Error::Preparation);
        }
    }

    SpellDraftHandle MagicAuthoring::latest() const noexcept { return latest_; }

    std::expected<Host::CastQuote, Error> MagicAuthoring::quote(EntityRef recipient) const noexcept {
        if (!latest_ || !latest_->accepted)
            return std::unexpected(Error::InvalidBody);
        return host_.quote(access_, *latest_->accepted, recipient);
    }

    std::expected<Action, Error> MagicAuthoring::action(EntityRef recipient) {
        const auto quoted = quote(recipient);
        if (!quoted)
            return std::unexpected(quoted.error());
        return host_.action(access_, *quoted);
    }
}
