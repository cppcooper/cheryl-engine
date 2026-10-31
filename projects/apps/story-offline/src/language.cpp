#include "language.h"

#include <charconv>

namespace OfflineStory {
    namespace {
        constexpr std::array<std::string_view, 20> identities = {"request",  "terms",   "proposal",  "helped",      "paid",
                                                                 "promised", "refused", "closing",   "ask-terms",   "help-now",
                                                                 "pay",      "promise", "negotiate", "commit",      "refuse",
                                                                 "close",    "deliver", "break",     "acknowledge", "leave"};
        constexpr std::array<std::string_view, 11> aftermath_ids = {"after.steward",    "after.ferry",       "after.reported",
                                                                    "after.reconciled", "after.booked",      "after.left",
                                                                    "after.report",     "after.visit-ferry", "after.book-passage",
                                                                    "after.reconcile",  "after.leave"};
        std::array<LanguageUnitDraft, 11> aftermath_units(unsigned variant) {
            constexpr std::string_view reported_en[] = {
                "Nera tells Mara she checked the delivery. Mara: A kept promise earns a one-coin return fare.",
                "Nera tells Mara she heard you declare the promise broken. Mara: The return fare is five coins. You can first pay Nera two coins in restitution to begin making amends.",
                "Nera tells Mara you declined without making a promise. Mara: That is no broken word. The return fare is three coins.",
                "Nera tells Mara she saw your immediate material help. Mara: Your help earns a two-coin return fare.",
                "Nera tells Mara she accepted your relief payment. Mara: Your help earns a two-coin return fare.",
                "Nera tells Mara only that a promise is still pending, with no checked delivery or declared breach. Mara: I will use the ordinary three-coin fare."
            };
            constexpr std::string_view reported_fr[] = {
                "Nera dit à Mara qu'elle a vérifié la livraison. Mara : Une promesse tenue donne droit à un retour pour une pièce.",
                "Nera dit à Mara qu'elle vous a entendu déclarer la promesse rompue. Mara : Le retour coûte cinq pièces. Vous pouvez d'abord verser deux pièces à Nera en réparation pour commencer à faire amende honorable.",
                "Nera dit à Mara que vous avez refusé sans rien promettre. Mara : Ce n'est pas une parole trahie. Le retour coûte trois pièces.",
                "Nera dit à Mara qu'elle a vu votre aide matérielle immédiate. Mara : Votre aide donne droit à un retour pour deux pièces.",
                "Nera dit à Mara qu'elle a accepté votre paiement pour l'aide. Mara : Votre aide donne droit à un retour pour deux pièces.",
                "Nera dit seulement à Mara qu'une promesse reste en attente, sans livraison vérifiée ni rupture déclarée. Mara : J'applique le tarif ordinaire de trois pièces."
            };
            // Whole utterances are authored together; never splice bytes or substitute mechanical quantities.
            constexpr std::string_view steward_full_en[] = {
                "Nera: I checked your delivery. Your promise was kept. Ask me to speak to Mara, visit her ferry desk, or leave.",
                "Nera: You declared the promise broken without delivering. I remember that. Ask me to speak to Mara, visit her ferry desk, or leave.",
                "Nera: You declined the bargain. There was no promise to break. Ask me to speak to Mara, visit her ferry desk, or leave.",
                "Nera: I saw your boards go into the work immediately. Ask me to speak to Mara, visit her ferry desk, or leave.",
                "Nera: I accepted your payment for the relief work. Ask me to speak to Mara, visit her ferry desk, or leave.",
                "Nera: Your recorded promise remains pending. I cannot report a checked delivery or a declared breach. Ask me to speak to Mara, visit her ferry desk, or leave."
            };
            constexpr std::string_view steward_full_fr[] = {
                "Nera : J'ai vérifié votre livraison. Vous avez tenu votre promesse. Demandez-moi de parler à Mara, allez à son guichet ou partez.",
                "Nera : Vous avez déclaré votre promesse rompue sans rien livrer. Je m'en souviens. Demandez-moi de parler à Mara, allez à son guichet ou partez.",
                "Nera : Vous avez refusé l'accord. Vous n'aviez fait aucune promesse à rompre. Demandez-moi de parler à Mara, allez à son guichet ou partez.",
                "Nera : J'ai vu vos planches servir immédiatement aux travaux. Demandez-moi de parler à Mara, allez à son guichet ou partez.",
                "Nera : J'ai accepté votre paiement pour les travaux d'aide. Demandez-moi de parler à Mara, allez à son guichet ou partez.",
                "Nera : Votre promesse reste en attente. Je ne peux attester ni une livraison vérifiée ni une rupture déclarée. Demandez-moi de parler à Mara, allez à son guichet ou partez."
            };
            return {
                {{aftermath_ids[0], steward_full_en[variant], steward_full_fr[variant]},
                 {aftermath_ids[1],
                  "Mara: I have no report about the relief work. A return seat costs three coins. Nera is nearby if you want her account.",
                  "Mara : Je n'ai aucun rapport sur les travaux d'aide. Une place pour le retour coûte trois pièces. Nera est tout près si vous souhaitez son témoignage."},
                 {aftermath_ids[2], reported_en[variant], reported_fr[variant]},
                 {aftermath_ids[3],
                  "Nera accepts two coins in restitution. The past breach remains recorded. Mara: You are making amends; the return fare is now three coins.",
                  "Nera accepte deux pièces en réparation. La rupture passée reste inscrite. Mara : Vous faites amende honorable ; le retour coûte désormais trois pièces."},
                 {aftermath_ids[4], "Mara takes the displayed fare. Your return passage is booked.",
                  "Mara reçoit le prix affiché. Votre passage de retour est réservé."},
                 {aftermath_ids[5], "You leave the quay without booking a return passage; earlier obligations are unchanged.",
                  "Vous quittez le quai sans réserver de passage de retour ; vos engagements antérieurs restent inchangés."},
                 {aftermath_ids[6], "Ask Nera to give Mara her witnessed account", "Demander à Nera de transmettre son témoignage à Mara"},
                 {aftermath_ids[7], "Visit Mara without a relief report", "Aller voir Mara sans rapport sur l'aide"},
                 {aftermath_ids[8], "Book the return passage at the displayed fare", "Réserver le passage de retour au prix affiché"},
                 {aftermath_ids[9], "Pay Nera two coins in restitution", "Verser deux pièces à Nera en réparation"},
                 {aftermath_ids[10], "Leave without booking a return passage", "Partir sans réserver de passage de retour"}}
            };
        }
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
    std::expected<LanguageCorpusDraft, LanguageError> aftermath_corpus(const AftermathContent& c) {
        const auto fact = static_cast<unsigned>(c.fact());
        if (fact < 1 || fact > 6 || !c.definition() || c.definition()->revision() != 1 || c.definition()->nodes().size() != 6 ||
            c.definition()->id().value.size() > 2048)
            return std::unexpected(LanguageError::Identity);
        LanguageCorpusDraft draft;
        draft.id = "aftermath-en-fr";
        draft.definition = c.definition()->id().value;
        draft.definition_revision = c.definition()->revision();
        draft.unit_count = 11;
        draft.variant = fact - 1;
        const auto units = aftermath_units(draft.variant);
        std::copy(units.begin(), units.end(), draft.units.begin());
        constexpr std::string_view nodes[] = {"steward", "ferry", "reported", "reconciled", "booked", "left"};
        constexpr std::string_view choices[] = {"report", "visit-ferry", "book-passage", "reconcile", "leave"};
        constexpr std::string_view offers[] = {
            "report,visit-ferry,leave,", "report,book-passage,leave,", "book-passage,reconcile,leave,", "book-passage,leave,", "", ""
        };
        for (std::size_t i = 0; i < 6; ++i) {
            const auto* node = c.definition()->find_node({std::string(nodes[i])});
            if (!node || node->text != units[i].english || node->choices.size() > 3)
                return std::unexpected(LanguageError::Identity);
            std::string actual;
            for (const auto& choice : node->choices) {
                bool found = false;
                for (std::size_t j = 0; j < 5; ++j)
                    if (choice.id.value == choices[j])
                        found = choice.text == units[j + 6].english;
                if (!found)
                    return std::unexpected(LanguageError::Identity);
                actual += choice.id.value + ',';
            }
            if (actual != offers[i])
                return std::unexpected(LanguageError::Identity);
        }
        return draft;
    }
    std::expected<RetainedLanguage, LanguageError> RetainedLanguage::retain(
        const LanguageCorpusDraft& d,
        const LanguageProfile& p,
        std::string_view definition,
        std::uint64_t revision,
        const LanguageLimits& limits
    ) {
        if (d.id != "relief-en-fr")
            return std::unexpected(LanguageError::Identity);
        return retain_checked(d, p, definition, revision, limits);
    }
    std::expected<RetainedLanguage, LanguageError> RetainedLanguage::retain(
        const LanguageCorpusDraft& d,
        const LanguageProfile& p,
        const AftermathContent& c,
        const LanguageLimits& limits
    ) {
        const auto source = aftermath_corpus(c);
        if (!source || d.id != "aftermath-en-fr" || d.variant != source->variant)
            return std::unexpected(LanguageError::Identity);
        return retain_checked(d, p, c.definition()->id().value, c.definition()->revision(), limits);
    }
    std::expected<RetainedLanguage, LanguageError> RetainedLanguage::retain_checked(
        const LanguageCorpusDraft& d,
        const LanguageProfile& p,
        std::string_view definition,
        std::uint64_t revision,
        const LanguageLimits& limits
    ) {
        const auto n = count(p);
        if (!n)
            return std::unexpected(LanguageError::Profile);
        for (auto field : {&LanguageLimits::units, &LanguageLimits::text_bytes, &LanguageLimits::storage_bytes, &LanguageLimits::work})
            if (limits.*field > maximum_language_limits.*field)
                return std::unexpected(LanguageError::Limit);
        if (d.id.size() > 32 || d.definition.size() > 2048 || definition.size() > 2048)
            return std::unexpected(LanguageError::Text);
        const bool after = d.id == "aftermath-en-fr";
        if ((!after && d.id != "relief-en-fr") || d.unit_count != (after ? 11 : 20) || d.variant > (after ? 5u : 0u) || d.revision != 1 ||
            d.selection_revision != 1 || d.definition != definition || d.definition_revision != revision || revision != 1)
            return std::unexpected(LanguageError::Identity);
        if (d.unit_count > limits.units || sizeof(RetainedLanguage) > limits.storage_bytes)
            return std::unexpected(LanguageError::Limit);
        RetainedLanguage r;
        if (!r.definition_.assign(d.definition) || !r.corpus_.assign(d.id))
            return std::unexpected(LanguageError::Text);
        r.definition_revision_ = revision;
        r.corpus_revision_ = d.revision;
        r.selection_revision_ = d.selection_revision;
        r.variant_ = d.variant;
        r.profile_ = p;
        r.unit_count_ = d.unit_count;
        r.target_count_ = after ? (d.unit_count * p.exposure + 50) / 100 : *n;
        r.usage_ = {d.definition.size() + d.id.size(), sizeof(RetainedLanguage), 0};
        const auto canonical = aftermath_units(d.variant);
        for (std::size_t i = 0; i < d.unit_count; ++i) {
            const auto& unit = d.units[i];
            if (unit.id != (after ? aftermath_ids[i] : identities[i]) || unit.revision != 1)
                return std::unexpected(LanguageError::Identity);
            auto& out = r.units_[i];
            if (!out.id.assign(unit.id) || !out.english.assign(unit.english) || !out.french.assign(unit.french))
                return std::unexpected(LanguageError::Text);
            if (after && (unit.english != canonical[i].english || unit.french != canonical[i].french))
                return std::unexpected(LanguageError::Identity);
            r.usage_.text_bytes += unit.id.size() + unit.english.size() + unit.french.size();
            out.target = i < r.target_count_; // Selection revision1: canonical ordinal prefix, never redraw a selection.
        }
        for (std::size_t i = d.unit_count; i < d.units.size(); ++i)
            if (!d.units[i].id.empty() || !d.units[i].english.empty() || !d.units[i].french.empty())
                return std::unexpected(LanguageError::Identity);
        // Logical validation/copy/meaning-comparison charge; fixed capacities bound even refused inputs.
        r.usage_.work = 3 * r.usage_.text_bytes + 20;
        if (r.usage_.text_bytes > limits.text_bytes || r.usage_.work > limits.work)
            return std::unexpected(LanguageError::Limit);
        return r;
    }
    std::expected<std::string_view, LanguageError> RetainedLanguage::text(std::string_view id, std::uint64_t revision, bool primary) const {
        if (id.size() > 24 || revision != 1)
            return std::unexpected(LanguageError::Identity);
        for (const auto& u : std::span(units_).first(unit_count_))
            if (u.id.view() == id) {
                const auto language = primary || !u.target ? profile_.primary : profile_.target;
                return language == Language::English ? u.english.view() : u.french.view();
            }
        return std::unexpected(LanguageError::Identity);
    }
    std::expected<std::string_view, LanguageError> RetainedLanguage::english(std::string_view id) const {
        for (const auto& u : std::span(units_).first(unit_count_))
            if (u.id.view() == id)
                return u.english.view();
        return std::unexpected(LanguageError::Identity);
    }
    std::expected<bool, LanguageError> RetainedLanguage::selected(std::string_view id) const {
        for (const auto& u : std::span(units_).first(unit_count_))
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
