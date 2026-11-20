#pragma once

#include "fragment.hpp"

#include <cstdint>

namespace CE::LivingWorld::Language {
    struct FieldAccess {
        bool disclosed{};
        Knowledge assistance{};
    };

    struct ExplanationAccess {
        bool content{};
        bool original{};
        bool origins{};
        bool status{};
        std::array<FieldAccess, 3> senses{};
        FieldAccess analysis;
        FieldAccess gloss;
        FieldAccess mechanics;
        FieldAccess repair;
    };

    struct TokenExplanation {
        ByteSpan span;
        std::string surface;
        std::optional<Origin> origin;
        std::optional<std::string> sense;
        bool operator==(const TokenExplanation&) const = default;
    };

    struct Explanation {
        std::optional<std::string> original;
        std::optional<std::string> language_label;
        std::vector<TokenExplanation> tokens;
        std::optional<std::string> status;
        std::optional<std::string> analysis;
        std::optional<std::string> gloss;
        std::vector<std::string> missing_mechanics;
        std::optional<std::string> repair;
        bool operator==(const Explanation&) const = default;
    };

    Explanation project(const Fragment& fragment, const Knowledge& character,
                        const ExplanationAccess& access, bool help);

    enum class Profile { Echelon5 = 5, Echelon15 = 15, Echelon40 = 40, Echelon75 = 75, Echelon100 = 100 };
    std::optional<Profile> profile_from_number(int value);
    enum class Locale { English, French };
    enum class PlayerFluency { Unreported, Novice, Fluent };
    struct DisplaySettings {
        bool help{};
        Locale locale{Locale::English};
        PlayerFluency player_fluency{PlayerFluency::Unreported};
    };

    struct PermittedScene {
        std::uint64_t audience{};
        std::uint64_t scene_incarnation{};
        std::uint64_t access_revision{};
        std::uint64_t meaning_revision{};
        std::uint64_t terms_revision{};
        std::string exact_terms;
        bool admitted{};
        bool ledger_disclosed{};
        bool may_request_ledger{};
        std::optional<bool> marin_willing;
        Profile profile{Profile::Echelon5};
        bool operator==(const PermittedScene&) const = default;
    };

    enum class Action { RequestLedgerInspection, OfferRelief, Leave };
    struct OfferToken {
        std::uint64_t owner{};
        std::uint64_t serial{};
        bool operator==(const OfferToken&) const = default;
    };
    struct PermittedChoice {
        Action action{};
        std::string label;
        bool enabled{};
        std::string tooltip;
        std::string assistance;
        bool operator==(const PermittedChoice&) const = default;
    };
    struct OfferedActions {
        OfferToken token;
        PermittedScene binding;
        std::vector<PermittedChoice> choices;
        bool operator==(const OfferedActions&) const = default;
    };
    struct RenderedChoice {
        Action action{};
        std::string label;
        bool enabled{};
        std::string tooltip;
        std::string accessibility;
        bool operator==(const RenderedChoice&) const = default;
    };
    struct RenderedOffer {
        OfferToken token;
        std::string exact_terms;
        Profile profile{};
        std::vector<RenderedChoice> choices;
        bool operator==(const RenderedOffer&) const = default;
    };
    RenderedOffer render(const OfferedActions& permitted, const DisplaySettings& display);
    enum class ChoiceResult { IntentOnly, StaleOffer, Unavailable };

    class OfferOwner {
        std::uint64_t owner_;
        std::uint64_t serial_{};
        std::optional<OfferedActions> live_;

    public:
        explicit OfferOwner(std::uint64_t owner);
        std::optional<OfferedActions> reconcile(const PermittedScene& current);
        ChoiceResult choose(OfferToken token, Action action, const PermittedScene& current);
    };
}
