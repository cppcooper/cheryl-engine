#include "drafts.hpp"

#include <algorithm>

namespace CE::LivingWorld::Language {
    std::optional<std::uint64_t> DraftStore::add(std::string_view input, Rule rule) {
        if (drafts_.size() + inscriptions_.size() >= Capacity) {
            return std::nullopt;
        }
        const auto id = next_id_;
        drafts_.push_back({id, analyze(input, rule)});
        ++next_id_;
        selected_ = DraftSelection{SelectionKind::Draft, id};
        return id;
    }

    std::optional<std::uint64_t> DraftStore::retain_practice_inscription(std::uint64_t draft) {
        const auto* source = find({SelectionKind::Draft, draft});
        if (!source || !source->fragment.patch || drafts_.size() + inscriptions_.size() >= Capacity) {
            return std::nullopt;
        }
        const auto id = next_id_;
        inscriptions_.push_back({id, source->fragment});
        ++next_id_;
        return id;
    }

    const Draft* DraftStore::find(DraftSelection selection) const {
        if (selection.kind != SelectionKind::Draft && selection.kind != SelectionKind::PriorInscription) {
            return nullptr;
        }
        const auto& records = selection.kind == SelectionKind::Draft ? drafts_ : inscriptions_;
        const auto found = std::ranges::find(records, selection.id, &Draft::id);
        return found == records.end() ? nullptr : &*found;
    }

    bool DraftStore::select(DraftSelection selection) {
        if (!find(selection)) {
            selected_.reset();
            return false;
        }
        selected_ = selection;
        return true;
    }

    CastRefusal DraftStore::cast(DraftSelection requested, const Knowledge& knowledge, const PermittedFeedback& permitted) const {
        CastRefusal refusal{requested, {}};
        if (!selected_) {
            refusal.reasons.push_back(Feedback::SelectionRequired);
            return refusal;
        }
        if (*selected_ != requested || !find(requested)) {
            refusal.reasons.push_back(Feedback::StaleSelection);
            return refusal;
        }
        const auto& fragment = find(requested)->fragment;
        if (permitted.syntax && fragment.status != AnalysisStatus::Incomplete) {
            refusal.reasons.push_back(Feedback::Syntax);
        }
        if (permitted.character_knowledge && !knows_all(knowledge, fragment.dependencies)) {
            refusal.reasons.push_back(Feedback::CharacterKnowledge);
        }
        if (permitted.mechanics && fragment.patch) {
            refusal.reasons.push_back(Feedback::MissingMechanics);
        }
        if (permitted.permission == false) {
            refusal.reasons.push_back(Feedback::Permission);
        }
        if (permitted.resources == false) {
            refusal.reasons.push_back(Feedback::Resources);
        }
        if (refusal.reasons.empty()) {
            refusal.reasons.push_back(Feedback::Undisclosed);
        }
        return refusal;
    }

    std::string_view feedback_text(Feedback feedback) {
        switch (feedback) {
            case Feedback::SelectionRequired: return "Select a draft or prior inscription deliberately; no cast occurred.";
            case Feedback::StaleSelection: return "That is not the selected draft or inscription; no cast occurred.";
            case Feedback::Syntax: return "This draft does not match its selected taught construction; no cast occurred.";
            case Feedback::CharacterKnowledge: return "Your character has not acquired the required lesson knowledge; no cast occurred.";
            case Feedback::MissingMechanics: return "The phrase supplies a motif, not complete supported spell mechanics; no cast occurred.";
            case Feedback::Permission: return "This invocation is not currently permitted or eligible; no new cast occurred.";
            case Feedback::Resources: return "The current permitted quote cannot be paid; no new cast occurred.";
            case Feedback::Undisclosed: return "This draft is not ready; no cast occurred.";
        }
        return "This draft is not ready; no cast occurred.";
    }
}
