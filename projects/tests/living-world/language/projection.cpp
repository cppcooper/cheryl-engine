#include "projection.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace CE::LivingWorld::Language {
    namespace {
        bool authorized(const Knowledge& character, const FieldAccess& field, bool help, const std::vector<Gate>& required) {
            return field.disclosed && std::ranges::all_of(required, [&](Gate gate) {
                return knows(character, gate) || (help && knows(field.assistance, gate));
            });
        }

        std::string status_text(AnalysisStatus status) {
            switch (status) {
                case AnalysisStatus::Incomplete: return "Uncastable draft; no cast occurred.";
                case AnalysisStatus::InvalidConstruction: return "No composed meaning accepted; no cast occurred.";
                case AnalysisStatus::UnsupportedInput: return "Input is outside the taught set; no cast occurred.";
                case AnalysisStatus::InvalidUtf8: return "Invalid UTF-8; no cast occurred.";
                case AnalysisStatus::Oversized: return "Input exceeds 256 bytes; no cast occurred.";
            }
            return "This draft is not ready; no cast occurred.";
        }

        std::string sense_text(Gate gate) {
            switch (gate) {
                case Gate::EnglishFire: return "Fire: noun, flames/burning, not the verb shoot.";
                case Gate::EnglishRing: return "Ring: circular arrangement, without a measured opening.";
                case Gate::FrenchCircle: return "Cercle: circle or circular arrangement.";
                case Gate::FrenchFire: return "Feu: noun, combustion/fire.";
                case Gate::FrenchWall: return "Mur: noun, wall.";
                case Gate::FrenchConstruction: return "De: linker within this specific form + de feu construction.";
                case Gate::MixedJoin: return "Hyphen: this lesson's game-specific mixed join.";
                default: return "No reviewed sense available.";
            }
        }
    }

    Explanation project(const Fragment& fragment, const Knowledge& character, const ExplanationAccess& access, bool help) {
        Explanation output;
        if (!access.content) {
            return output;
        }
        if (access.status) {
            output.status = status_text(fragment.status);
        }
        const bool analyzed = fragment.status == AnalysisStatus::Incomplete || fragment.status == AnalysisStatus::InvalidConstruction;
        if (access.original && valid_utf8(fragment.original_utf8) && fragment.status != AnalysisStatus::Oversized) {
            output.original = fragment.original_utf8;
        }
        if (!analyzed) {
            return output;
        }
        if (access.original) {
            if (access.origins) {
                output.language_label = fragment.language_label;
            }
            for (std::size_t index = 0; index < fragment.tokens.size() && index < access.senses.size(); ++index) {
                const auto& token = fragment.tokens[index];
                TokenExplanation visible{token.span, token.surface, std::nullopt, std::nullopt};
                if (access.origins) {
                    visible.origin = token.origin;
                }
                if (authorized(character, access.senses[index], help, {token.gate})) {
                    visible.sense = sense_text(token.gate);
                }
                output.tokens.push_back(std::move(visible));
            }
        }
        if (authorized(character, access.analysis, help, fragment.dependencies)) {
            output.analysis = fragment.bracket;
        }
        if (authorized(character, access.gloss, help, fragment.dependencies)) {
            output.gloss = fragment.gloss;
        }
        if (fragment.patch && authorized(character, access.mechanics, help, fragment.dependencies)) {
            for (const auto slot : fragment.unresolved) {
                output.missing_mechanics.emplace_back(slot_name(slot));
            }
        }
        if (fragment.status == AnalysisStatus::InvalidConstruction &&
            authorized(character, access.repair, help, fragment.dependencies)) {
            output.repair = fragment.gloss;
        }
        return output;
    }

    std::optional<Profile> profile_from_number(int value) {
        switch (value) {
            case 5: return Profile::Echelon5;
            case 15: return Profile::Echelon15;
            case 40: return Profile::Echelon40;
            case 75: return Profile::Echelon75;
            case 100: return Profile::Echelon100;
            default: return std::nullopt;
        }
    }

    OfferOwner::OfferOwner(std::uint64_t owner) : owner_(owner) {
        if (owner == 0) {
            throw std::invalid_argument("Offer owner scope must be nonzero");
        }
    }

    std::optional<OfferedActions> OfferOwner::reconcile(const PermittedScene& current) {
        if (!current.admitted || current.audience == 0 || current.scene_incarnation == 0 ||
            !profile_from_number(static_cast<int>(current.profile))) {
            live_.reset();
            return std::nullopt;
        }
        if (current.exact_terms.size() > MaxInputBytes) {
            live_.reset();
            return std::nullopt;
        }
        if (live_ && live_->binding == current) {
            return live_;
        }
        if (serial_ == std::numeric_limits<std::uint64_t>::max()) {
            throw std::overflow_error("Offer identity exhausted");
        }
        OfferedActions next{{owner_, serial_ + 1}, current, {}};
        if (current.ledger_disclosed) {
            next.choices.push_back({Action::RequestLedgerInspection, "Ask to inspect the clinic ledger", current.may_request_ledger,
                                    current.may_request_ledger ? "This requests access, not a finding."
                                                               : "Ledger access cannot currently be requested.",
                                    "This asks for access; it does not establish what happened."});
        }
        const bool willing = current.marin_willing.value_or(false);
        next.choices.push_back({Action::OfferRelief, "Offer to help Marin", willing,
                                willing ? "This offers help; no effect has occurred."
                                        : current.marin_willing.has_value() ? "Marin has not agreed to magical help."
                                                                          : "No permitted agreement is available.",
                                "An offer is not a cast or an acquired claim."});
        next.choices.push_back({Action::Leave, "Leave", true, "No obligation is created.", "Leaving does not establish what happened."});
        live_ = std::move(next);
        ++serial_;
        return live_;
    }

    ChoiceResult OfferOwner::choose(OfferToken token, Action action, const PermittedScene& current) {
        const auto offered = reconcile(current);
        if (!offered || offered->token != token) {
            return ChoiceResult::StaleOffer;
        }
        const auto choice = std::ranges::find(offered->choices, action, &PermittedChoice::action);
        if (choice == offered->choices.end() || !choice->enabled) {
            return ChoiceResult::Unavailable;
        }
        return ChoiceResult::IntentOnly;
    }

    RenderedOffer render(const OfferedActions& permitted, const DisplaySettings& display) {
        RenderedOffer output{permitted.token, permitted.binding.exact_terms, permitted.binding.profile, {}};
        for (const auto& choice : permitted.choices) {
            std::string tooltip = choice.tooltip;
            if (display.help && !choice.assistance.empty()) {
                tooltip += " " + choice.assistance;
            }
            output.choices.push_back({choice.action, choice.label, choice.enabled, tooltip, choice.label + ". " + tooltip});
        }
        return output;
    }
}
