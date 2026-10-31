#include "language.h"

#include <charconv>

namespace OfflineStory {
    namespace {
        constexpr std::array<std::string_view, 20> identities = {"request",  "terms",   "proposal",  "helped",      "paid",
                                                                 "promised", "refused", "closing",   "ask-terms",   "help-now",
                                                                 "pay",      "promise", "negotiate", "commit",      "refuse",
                                                                 "close",    "deliver", "break",     "acknowledge", "leave"};
        std::optional<std::size_t> count(const LanguageProfile& p) {
            if (p.interface_locale != Language::English || p.primary == p.target ||
                (p.primary != Language::English && p.primary != Language::French) ||
                (p.target != Language::English && p.target != Language::French))
                return {};
            switch (p.exposure) {
                case 5:
                    return 1;
                case 15:
                    return 3;
                case 40:
                    return 8;
                case 75:
                    return 15;
                case 100:
                    return 20;
                default:
                    return {};
            }
        }
        constexpr std::string_view nodes_en[2][7] = {
            {"The steward explains the need. An interim repair covers the urgent gap, so a smaller later bundle is acceptable.",
             "With the details known, a revised bargain is possible. Choose the smaller pending obligation, help now instead, or walk away.",
             "The steward puts your material to work immediately.", "The steward accepts the approved payment.",
             "A pending obligation is recorded. No material or future reward has been delivered.",
             "The steward is disappointed but accepts your refusal.", "The relief conversation closes on this bank."},
            {"The steward shares the practical details. The urgent work is small; staged help can reduce the later promise.",
             "The new information opens a smaller commitment. The revised offer waits for your explicit commitment; nothing is delivered yet.",
             "Your boards join the work today.", "Your coins fund today's work.",
             "The steward records your word, not a completed delivery. The reward must wait.", "You part without a delivery or a debt.",
             "The steward returns to the work on this bank."}
        };
        constexpr std::string_view nodes_fr[2][7] = {
            {"L'intendante explique les besoins. Une réparation provisoire suffit pour l'urgence ; un lot plus petit pourra donc être livré plus tard.",
             "Ces précisions permettent de revoir l'accord. Choisissez un engagement réduit, aidez maintenant ou partez.",
             "L'intendante utilise immédiatement vos matériaux.", "L'intendante accepte le paiement convenu.",
             "Un engagement est enregistré. Aucun matériau ni aucune récompense future n'a encore été remis.",
             "L'intendante est déçue mais accepte votre refus.", "La discussion sur l'aide se termine sur cette rive."},
            {"L'intendante donne les détails pratiques. Les travaux urgents sont modestes ; une aide par étapes peut réduire la livraison promise.",
             "Ces informations permettent un engagement réduit. L'offre révisée attend votre accord explicite ; rien n'est encore livré.",
             "Vos planches servent aux travaux aujourd'hui.", "Vos pièces financent les travaux d'aujourd'hui.",
             "L'intendante enregistre votre parole, pas une livraison effectuée. La récompense doit attendre.",
             "Vous vous séparez sans livraison ni dette.", "L'intendante reprend les travaux sur cette rive."}
        };
        constexpr std::string_view choices_en[2][8] = {
            {"Ask for the exact terms", "Provide material help now", "Pay the approved relief cost", "Promise the full later bundle",
             "Negotiate a smaller later bundle", "Commit to the smaller pending promise", "Refuse the bargain", "Close this conversation"},
            {"Hear what the repair really needs", "Put boards into the work today", "Fund today's work with coins",
             "Take on the original obligation", "Use the learned terms to revise the promise", "Accept the revised obligation",
             "Keep your supplies and make no promise", "Close this conversation"}
        };
        constexpr std::string_view choices_fr[2][8] = {
            {"Demander les conditions exactes", "Fournir des matériaux maintenant", "Payer le coût convenu de l'aide",
             "Promettre le lot complet pour plus tard", "Négocier un lot plus petit pour plus tard", "Accepter l'engagement réduit",
             "Refuser l'accord", "Terminer cette discussion"},
            {"Écouter les besoins réels de la réparation", "Donner des planches pour les travaux aujourd'hui",
             "Financer les travaux avec des pièces", "Accepter l'engagement initial", "Revoir la promesse grâce aux conditions apprises",
             "Accepter l'engagement révisé", "Garder vos provisions sans rien promettre", "Terminer cette discussion"}
        };
        constexpr std::string_view payoff_en[] = {
            "Deliver the recorded material", "Declare the promise broken", "Hear the steward's response", "Leave without settling"
        };
        constexpr std::string_view payoff_fr[] = {
            "Livrer les matériaux convenus", "Déclarer la promesse rompue", "Écouter la réponse de l'intendante",
            "Partir sans régler l'affaire"
        };
    }
    bool valid_utf8(std::string_view s, bool command) noexcept {
        for (std::size_t i = 0; i < s.size();) {
            const auto lead = static_cast<unsigned char>(s[i++]);
            if (lead < 128) {
                if (lead < 32 || lead == 127) {
                    if (!command || (lead != '\t' && lead != '\r'))
                        return false;
                }
                continue;
            }
            unsigned remaining = 0, scalar = 0, minimum = 0;
            if (lead >= 0xc2 && lead <= 0xdf) {
                remaining = 1;
                scalar = lead & 0x1f;
                minimum = 0x80;
            } else if (lead >= 0xe0 && lead <= 0xef) {
                remaining = 2;
                scalar = lead & 0x0f;
                minimum = 0x800;
            } else if (lead >= 0xf0 && lead <= 0xf4) {
                remaining = 3;
                scalar = lead & 7;
                minimum = 0x10000;
            } else
                return false;
            if (remaining > s.size() - i)
                return false;
            while (remaining--) {
                const auto next = static_cast<unsigned char>(s[i++]);
                if ((next & 0xc0) != 0x80)
                    return false;
                scalar = (scalar << 6) | (next & 0x3f);
            }
            if (scalar < minimum || scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff) || (scalar >= 0x80 && scalar <= 0x9f) ||
                scalar == 0x2028 || scalar == 0x2029)
                return false;
        }
        return true;
    }
    std::expected<LanguageProfile, LanguageError>
    language_profile(std::string_view primary, std::string_view target, std::string_view exposure, bool assistance) {
        const auto parse = [](std::string_view s) -> std::optional<Language> {
            if (s == "en")
                return Language::English;
            if (s == "fr")
                return Language::French;
            return {};
        };
        const auto a = parse(primary), b = parse(target);
        if (!a || !b || exposure.empty() || exposure.size() > 3)
            return std::unexpected(LanguageError::Profile);
        unsigned tier = 0;
        const auto number = std::from_chars(exposure.data(), exposure.data() + exposure.size(), tier);
        if (number.ec != std::errc{} || number.ptr != exposure.data() + exposure.size())
            return std::unexpected(LanguageError::Profile);
        LanguageProfile p{Language::English, *a, *b, tier, assistance};
        if (!count(p))
            return std::unexpected(LanguageError::Profile);
        return p;
    }
    std::expected<std::string, LanguageError> language_label(const LanguageProfile& p, std::optional<std::string_view> blend) {
        if (!count(p) || (blend && (blend->size() > 64 || !valid_utf8(*blend) || blend->empty())))
            return std::unexpected(LanguageError::Profile);
        const auto primary = p.primary == Language::English ? "English" : "Français";
        const auto target = p.primary == Language::English ? "French" : "Anglais";
        std::string label = p.exposure == 100 ? target : blend ? std::string(*blend) : std::string(primary) + " + " + target;
        return label + " · " + std::to_string(p.exposure) + "%";
    }
    std::expected<LanguageCorpusDraft, LanguageError> relief_corpus(const BargainArtifact& c) {
        const auto f = c.pick().flavour;
        if (f > 1 || c.sets().id != "relief-sets" || c.sets().revision != 1 || c.catalogue().id != "harbor-relief" ||
            c.catalogue().revision != 1 || c.definition()->revision() != 1 || c.definition()->nodes().size() != 8 ||
            c.request().location != ReliefLocation::Quay)
            return std::unexpected(LanguageError::Identity);
        // This playable preset is always on the quay. Other locations/packs require separately authored coverage.
        const bool ramp = c.request().proposition == "ramp-request";
        if (!ramp && c.request().proposition != "shelter-request")
            return std::unexpected(LanguageError::Identity);
        constexpr std::string_view request_en[2][2] = {
            {"On the harbor quay. The relief ramp needs a sound brace before nightfall.",
             "On the harbor quay. A loose ramp keeps the relief carts waiting."},
            {"On the harbor quay. The relief shelter needs boards before the rain.",
             "On the harbor quay. Rain is coming through the shelter roof."}
        };
        constexpr std::string_view request_fr[2][2] = {
            {"Sur le quai du port. La rampe d'aide a besoin d'un appui solide avant la nuit.",
             "Sur le quai du port. Une rampe instable bloque les chariots d'aide."},
            {"Sur le quai du port. L'abri d'aide a besoin de planches avant la pluie.",
             "Sur le quai du port. La pluie traverse le toit de l'abri."}
        };
        LanguageCorpusDraft d;
        d.definition = c.definition()->id().value;
        d.definition_revision = c.definition()->revision();
        d.units[0] = {identities[0], request_en[ramp ? 0 : 1][f], request_fr[ramp ? 0 : 1][f]};
        for (std::size_t i = 1; i < 8; ++i)
            d.units[i] = {identities[i], nodes_en[f][i - 1], nodes_fr[f][i - 1]};
        for (std::size_t i = 8; i < 16; ++i)
            d.units[i] = {identities[i], choices_en[f][i - 8], choices_fr[f][i - 8]};
        for (std::size_t i = 16; i < 20; ++i)
            d.units[i] = {identities[i], payoff_en[i - 16], payoff_fr[i - 16]};
        // Refuse changed meaning/revision rather than placing old French over newly authored English.
        for (std::size_t i = 0; i < 8; ++i) {
            const auto* node = c.definition()->find_node({std::string(identities[i])});
            if (!node || node->text != d.units[i].english)
                return std::unexpected(LanguageError::Identity);
            for (const auto& choice : node->choices) {
                bool found = false;
                for (std::size_t j = 8; j < 16; ++j)
                    if (choice.id.value == identities[j])
                        found = choice.text == d.units[j].english;
                if (!found)
                    return std::unexpected(LanguageError::Identity);
            }
        }
        return d;
    }
    std::expected<RetainedLanguage, LanguageError>
    RetainedLanguage::retain(const LanguageCorpusDraft& d, const LanguageProfile& p, std::string_view definition, std::uint64_t revision) {
        const auto n = count(p);
        if (!n)
            return std::unexpected(LanguageError::Profile);
        if (d.id != "relief-en-fr" || d.revision != 1 || d.selection_revision != 1 || d.definition != definition ||
            d.definition_revision != revision || revision != 1)
            return std::unexpected(LanguageError::Identity);
        RetainedLanguage r;
        if (!r.definition_.assign(d.definition))
            return std::unexpected(LanguageError::Text);
        r.definition_revision_ = revision;
        r.profile_ = p;
        r.target_count_ = *n;
        for (std::size_t i = 0; i < 20; ++i) {
            const auto& unit = d.units[i];
            if (unit.id != identities[i] || unit.revision != 1)
                return std::unexpected(LanguageError::Identity);
            auto& out = r.units_[i];
            if (!out.id.assign(unit.id) || !out.english.assign(unit.english) || !out.french.assign(unit.french))
                return std::unexpected(LanguageError::Text);
            out.target = i < *n; // Selection revision1: canonical ordinal prefix, never draw again at render time.
        }
        return r;
    }
    std::expected<std::string_view, LanguageError> RetainedLanguage::text(std::string_view id, std::uint64_t revision, bool primary) const {
        if (id.size() > 24 || revision != 1)
            return std::unexpected(LanguageError::Identity);
        for (const auto& u : units_)
            if (u.id.view() == id) {
                const auto language = primary || !u.target ? profile_.primary : profile_.target;
                return language == Language::English ? u.english.view() : u.french.view();
            }
        return std::unexpected(LanguageError::Identity);
    }
    std::expected<std::string_view, LanguageError> RetainedLanguage::english(std::string_view id) const {
        for (const auto& u : units_)
            if (u.id.view() == id)
                return u.english.view();
        return std::unexpected(LanguageError::Identity);
    }
    std::expected<bool, LanguageError> RetainedLanguage::selected(std::string_view id) const {
        for (const auto& u : units_)
            if (u.id.view() == id)
                return u.target;
        return std::unexpected(LanguageError::Identity);
    }
    std::string RetainedLanguage::label() const {
        // Only English/French has an authored blend. Reverse uses the primary-localized fallback.
        return language_label(profile_, profile_.primary == Language::English ? std::optional<std::string_view>("Enfrench") : std::nullopt)
            .value();
    }
}
