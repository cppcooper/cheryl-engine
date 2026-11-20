#include "drafts.hpp"
#include "practice.hpp"
#include "projection.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
    using namespace CE::LivingWorld::Language;

    void require(bool condition, std::string_view message) {
        if (!condition) {
            throw std::runtime_error(std::string(message));
        }
    }

    Knowledge all_knowledge() {
        Knowledge knowledge{};
        knowledge.fill(true);
        return knowledge;
    }

    ExplanationAccess teaching_access() {
        ExplanationAccess access;
        access.content = access.original = access.origins = access.status = true;
        auto permit = [](FieldAccess& field) {
            field.disclosed = true;
            field.assistance.fill(true);
        };
        for (auto& sense : access.senses) { permit(sense); }
        permit(access.analysis);
        permit(access.gloss);
        permit(access.mechanics);
        permit(access.repair);
        return access;
    }

    void fragments() {
        struct Expected {
            std::string_view text;
            Rule rule;
            AnalysisStatus status;
            std::vector<ByteSpan> spans;
            std::optional<std::size_t> head;
            std::optional<std::size_t> modifier;
            std::optional<std::size_t> link;
            std::optional<FragmentPatch> patch;
        };
        const std::array examples{
            Expected{"fire", Rule::EnglishNoun, AnalysisStatus::Incomplete, {{0, 4}}, 0, {}, {}, FragmentPatch{}},
            Expected{"fire ring", Rule::EnglishCompound, AnalysisStatus::Incomplete, {{0, 4}, {5, 9}}, 1, 0, {},
                     FragmentPatch{Form::CircularArrangement, Relation::SelectedFireMaterial}},
            Expected{"cercle de feu", Rule::FrenchForm, AnalysisStatus::Incomplete, {{0, 6}, {7, 9}, {10, 13}}, 0, 2, 1,
                     FragmentPatch{Form::CircularArrangement, Relation::FieryMaterialAppearance}},
            Expected{"feu-ring", Rule::MixedEnglish, AnalysisStatus::Incomplete, {{0, 3}, {3, 4}, {4, 8}}, 2, 0, 1,
                     FragmentPatch{Form::CircularArrangement, Relation::SelectedFireMaterial}},
            Expected{"feu cercle", Rule::FrenchForm, AnalysisStatus::InvalidConstruction, {{0, 3}, {4, 10}}, {}, {}, {}, {}},
            Expected{"cercle feu", Rule::FrenchForm, AnalysisStatus::InvalidConstruction, {{0, 6}, {7, 10}}, {}, {}, {}, {}},
            Expected{"mur de feu", Rule::FrenchForm, AnalysisStatus::Incomplete, {{0, 3}, {4, 6}, {7, 10}}, 0, 2, 1,
                     FragmentPatch{Form::Wall, Relation::FieryMaterialAppearance}}
        };
        for (const auto& expected : examples) {
            const auto actual = analyze(expected.text, expected.rule);
            require(actual.original_utf8 == expected.text && actual.submitted_bytes == expected.text.size(), "exact original bytes");
            require(actual.status == expected.status && !actual.castable(), "honest incomplete or invalid status");
            require(actual.patch == expected.patch, "typed motif and exact relation");
            require(actual.head == expected.head && actual.modifier == expected.modifier && actual.link == expected.link,
                    "exact head/modifier/link references");
            require(actual.tokens.size() == expected.spans.size(), "token count");
            std::size_t previous_end = 0;
            for (std::size_t index = 0; index < actual.tokens.size(); ++index) {
                const auto& token = actual.tokens[index];
                require(token.span == expected.spans[index], "exact byte spans");
                require(token.span.begin >= previous_end && token.span.end <= actual.original_utf8.size(), "ordered bounded spans");
                require(actual.original_utf8.substr(token.span.begin, token.span.end - token.span.begin) == token.surface,
                        "owned surface matches byte slice");
                require(!token.claims.empty(), "retained token source claims");
                previous_end = token.span.end;
            }
            require(actual.unresolved.size() >= 8, "mechanics stay unresolved");
            require(std::ranges::find(actual.unresolved, Slot::Price) != actual.unresolved.end(), "no invented price");
            require(std::ranges::find(actual.unresolved, Slot::Trigger) != actual.unresolved.end(), "no invented hit trigger");
        }
        const auto english = analyze("fire ring", Rule::EnglishCompound);
        const auto french = analyze("cercle de feu", Rule::FrenchForm);
        const auto mixed = analyze("feu-ring", Rule::MixedEnglish);
        const auto wall = analyze("mur de feu", Rule::FrenchForm);
        require(english.patch->form == french.patch->form && english.patch->fire == french.patch->fire, "coarse motif agrees");
        require(english.patch != french.patch && english != french, "motif is not full patch or provenance equality");
        require(english.patch == mixed.patch && english != mixed, "mixed provenance and convention remain distinct");
        require(english.construction_claims == std::vector<std::string>{"EN-G1/G2"}, "compound source claim");
        require(french.construction_claims == std::vector<std::string>{"FR-G1", "FR-G2", "FR-C3"}, "historical circle claim retained");
        require(wall.construction_claims == std::vector<std::string>{"FR-G1", "FR-G2"}, "wall does not borrow circle attestation");
        require(wall.patch->relation == french.patch->relation && wall.patch->form != french.patch->form, "head replacement changes form");
        require(mixed.tokens[0].origin == Origin::French && mixed.tokens[2].origin == Origin::English, "mixed lexical origins");
        require(std::ranges::find(mixed.dependencies, Gate::MixedJoin) != mixed.dependencies.end(), "mixed join dependency");
        require(analyze("feu-ring", Rule::FrenchForm).status == AnalysisStatus::UnsupportedInput, "no silent governing grammar switch");
    }

    void input_rejections() {
        for (const auto phrase : {"", "Fire", " fire", "fire ", "fire  ring", "fire\tring", "fire-ring", "feu", "mur feu",
                                  "cercle de feu!", "cercle de feu ring", "unknown"}) {
            const auto fragment = analyze(phrase, Rule::FrenchForm);
            require(fragment.status == AnalysisStatus::UnsupportedInput && !fragment.patch && fragment.tokens.empty(), "unsupported exact input");
            require(fragment.original_utf8 == phrase, "no trim, normalization, or correction");
        }
        const std::array<std::string, 8> invalid{
            "\x80", "\xc0\xaf", "\xc2", "\xe2\x28\xa1", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xf5\x80\x80\x80", "\xe0\x80\xaf"
        };
        for (const auto& bytes : invalid) {
            const auto fragment = analyze(bytes, Rule::FrenchForm);
            require(!valid_utf8(bytes) && fragment.status == AnalysisStatus::InvalidUtf8, "strict UTF-8 rejection");
            require(fragment.original_utf8 == bytes && !fragment.patch && fragment.tokens.empty(), "invalid bytes preserved, no tokenization");
        }
        for (const auto& bytes : {std::string("\xc2\xa2"), std::string("m\xc3\xbbr"), std::string("\xe2\x82\xac"),
                                  std::string("\xf0\x9f\x94\xa5"), std::string("\xf4\x8f\xbf\xbf")}) {
            require(valid_utf8(bytes), "valid multibyte Unicode");
            const auto fragment = analyze(bytes, Rule::FrenchForm);
            require(fragment.original_utf8 == bytes && fragment.status == AnalysisStatus::UnsupportedInput, "valid Unicode is not translated");
        }
        const auto nul = analyze(std::string("fire\0ring", 9), Rule::EnglishCompound);
        require(nul.original_utf8.size() == 9 && nul.status == AnalysisStatus::UnsupportedInput, "embedded NUL is not a terminator");
        require(analyze(std::string(MaxInputBytes, 'x'), Rule::EnglishNoun).status == AnalysisStatus::UnsupportedInput, "inclusive byte bound");
        const auto oversized = analyze(std::string(MaxInputBytes + 1, 'x'), Rule::EnglishNoun);
        require(oversized.status == AnalysisStatus::Oversized && oversized.original_utf8.empty(), "oversized input is rejected, not truncated");
        require(oversized.submitted_bytes == MaxInputBytes + 1 && !oversized.patch, "oversized size recorded");
    }

    void lifetime() {
        Fragment retained;
        Explanation projected;
        {
            std::string input = "cercle de feu";
            retained = analyze(input, Rule::FrenchForm);
            projected = project(retained, all_knowledge(), teaching_access(), false);
            input.assign("destroyed");
        }
        require(retained.original_utf8 == "cercle de feu" && retained.tokens[2].surface == "feu", "owning analysis after caller destruction");
        retained = Fragment{};
        require(projected.original == "cercle de feu" && projected.analysis == "[cercle [de feu]]", "owning projected values after producer destruction");
    }

    void no_fallback() {
        DraftStore store;
        const auto first = store.add("fire ring", Rule::EnglishCompound);
        require(first.has_value(), "first draft accepted");
        const auto prior = store.retain_practice_inscription(*first);
        require(prior.has_value(), "immutable prior practice inscription retained");
        const DraftSelection accepted{SelectionKind::PriorInscription, *prior};
        const auto original = store.find(accepted)->fragment;
        require(store.select(accepted), "explicit prior selection");
        const auto failed = store.add("cercle feu", Rule::FrenchForm);
        require(failed.has_value(), "failed draft has its own identity");
        const DraftSelection bad{SelectionKind::Draft, *failed};
        const PermittedFeedback disclosed{true, true, true, std::nullopt, std::nullopt};
        const auto result = store.cast(bad, all_knowledge(), disclosed);
        require(result.requested == bad && result.reasons == std::vector{Feedback::Syntax}, "failed D2 refuses rather than falling back");
        require(store.find(accepted)->fragment == original && store.selected() == bad, "A1 preserved, D2 remains selected");
        require(store.cast(accepted, all_knowledge(), disclosed).reasons == std::vector{Feedback::StaleSelection}, "stale A1 request refuses");
        require(!store.retain_practice_inscription(*failed), "failed draft cannot become prior inscription");
        require(store.select(accepted), "deliberate A1 selection required");
        require(store.cast(accepted, all_knowledge(), disclosed).reasons == std::vector{Feedback::MissingMechanics}, "even A1 is only a motif");
        const auto repaired = store.add("cercle de feu", Rule::FrenchForm);
        require(repaired.has_value() && *repaired != *failed, "repair creates a new immutable draft identity");
        require(store.cast(bad, all_knowledge(), disclosed).reasons == std::vector{Feedback::StaleSelection}, "old failed D2 remains stale after repair");
        require(store.find(bad)->fragment.status == AnalysisStatus::InvalidConstruction, "repair never mutates D2");
        require(!store.select({SelectionKind::Draft, 9000}) && !store.selected(), "failed explicit selection clears selection");
        require(store.cast(accepted, all_knowledge(), disclosed).reasons == std::vector{Feedback::SelectionRequired}, "cleared selection cannot fall back");
        while (store.add("fire", Rule::EnglishNoun)) {}
        require(!store.add("fire", Rule::EnglishNoun), "bounded storage refuses overflow");
        require(store.find(accepted)->fragment == original, "capacity failure preserves prior inscription");
    }

    void disclosure() {
        const auto fragment = analyze("cercle de feu", Rule::FrenchForm);
        const Knowledge unknown{};
        const auto access = teaching_access();
        require(project(fragment, all_knowledge(), {}, true) == Explanation{}, "scene access denies every field");
        const auto unassisted = project(fragment, unknown, access, false);
        require(unassisted.tokens.size() == 3 && !unassisted.tokens[0].sense && !unassisted.gloss && !unassisted.analysis,
                "origin disclosure is not lexical knowledge");
        const auto assisted = project(fragment, unknown, access, true);
        require(assisted.gloss && assisted.analysis && assisted.missing_mechanics.size() == 8, "authorized field assistance");
        require(unknown == Knowledge{}, "help does not mutate acquired knowledge");
        auto partial = access;
        partial.senses[0].assistance.fill(false);
        partial.gloss.disclosed = false;
        partial.analysis.assistance[static_cast<std::size_t>(Gate::FrenchMotif)] = false;
        partial.mechanics.disclosed = false;
        const auto limited = project(fragment, unknown, partial, true);
        require(!limited.tokens[0].sense && limited.tokens[2].sense && !limited.gloss && !limited.analysis &&
                limited.missing_mechanics.empty(), "each field has independent assistance and disclosure");
        auto known_fire = unknown;
        known_fire[static_cast<std::size_t>(Gate::FrenchFire)] = true;
        const auto lexical = project(fragment, known_fire, access, false);
        require(!lexical.tokens[0].sense && lexical.tokens[2].sense && !lexical.gloss, "known token cannot expose neighbor meaning");
        auto no_text = access;
        no_text.original = false;
        no_text.origins = false;
        const auto textless = project(fragment, unknown, no_text, false);
        require(!textless.original && !textless.language_label && textless.tokens.empty(), "text and origin metadata withheld");
        const auto mixed = analyze("feu-ring", Rule::MixedEnglish);
        auto no_join = all_knowledge();
        no_join[static_cast<std::size_t>(Gate::MixedJoin)] = false;
        require(!project(mixed, no_join, access, false).gloss, "mixed convention knowledge is required");
        const auto bad = analyze("cercle feu", Rule::FrenchForm);
        require(!project(bad, unknown, access, false).repair && project(bad, unknown, access, true).repair,
                "specific repairs require authorized teaching");
        require(project(analyze("\xff", Rule::FrenchForm), unknown, access, true).tokens.empty(), "invalid UTF-8 has no rendered token");
    }

    PermittedScene scene() {
        return {7, 4, 1, 1, 2, "Request access only; no payment or effect", true, true, true, false, Profile::Echelon75};
    }

    void offers() {
        OfferOwner owner(17);
        const auto initial = scene();
        const auto live = owner.reconcile(initial);
        require(live && live->choices.size() == 3, "permitted authored three-choice offer");
        const auto before = render(*live, {});
        const auto helped = render(*live, {true, Locale::French, PlayerFluency::Fluent});
        require(before.token == helped.token && before.exact_terms == helped.exact_terms && before.profile == helped.profile,
                "help/locale/fluency preserve offer, terms, and social profile");
        for (std::size_t index = 0; index < before.choices.size(); ++index) {
            require(before.choices[index].action == helped.choices[index].action &&
                    before.choices[index].enabled == helped.choices[index].enabled, "display cannot change actions or eligibility");
        }
        require(owner.reconcile(initial) == live, "rerender does not reissue offer");
        require(owner.choose(live->token, Action::OfferRelief, initial) == ChoiceResult::Unavailable, "disabled intent refused");
        require(owner.choose(live->token, Action::RequestLedgerInspection, initial) == ChoiceResult::IntentOnly, "leaf intent, not Dialogue selection/effect");
        for (int mutation = 0; mutation < 6; ++mutation) {
            auto changed = initial;
            switch (mutation) {
                case 0: ++changed.terms_revision; break;
                case 1: ++changed.access_revision; break;
                case 2: ++changed.meaning_revision; break;
                case 3: ++changed.scene_incarnation; break;
                case 4: ++changed.audience; break;
                case 5: changed.exact_terms = "Changed exact terms without a revision bump"; break;
            }
            OfferOwner fresh(20 + static_cast<std::uint64_t>(mutation));
            const auto old = fresh.reconcile(initial);
            require(fresh.choose(old->token, Action::Leave, changed) == ChoiceResult::StaleOffer, "changed binding rejects old token");
            const auto replacement = fresh.reconcile(changed);
            require(replacement && replacement->token != old->token, "changed binding reissues value");
            require(fresh.choose(replacement->token, Action::Leave, changed) == ChoiceResult::IntentOnly, "new token usable");
        }
        auto revoked = initial;
        revoked.admitted = false;
        require(owner.choose(live->token, Action::Leave, revoked) == ChoiceResult::StaleOffer, "revoked access rejects old offer");
        require(!owner.reconcile(revoked), "fluent outsider has no admitted offer");
        const auto restored = owner.reconcile(initial);
        require(restored && restored->token != live->token, "restored admission cannot resurrect old offer");
        OfferOwner other(18);
        require(other.reconcile(initial).has_value(), "second owner setup");
        require(other.choose(restored->token, Action::Leave, initial) == ChoiceResult::StaleOffer, "wrong owner token rejected");
        auto excessive = initial;
        excessive.exact_terms.assign(MaxInputBytes + 1, 'x');
        require(!owner.reconcile(excessive), "bounded offer terms");
        for (const auto tier : {5, 15, 40, 75, 100}) {
            require(profile_from_number(tier).has_value(), "all five social profiles");
        }
        require(!profile_from_number(0) && !profile_from_number(50), "no zero or invented tier");
    }

    void two_worlds() {
        struct PrivateWorld { std::string culprit; PermittedScene permitted; };
        const PrivateWorld first{"Iona", scene()};
        const PrivateWorld second{"Pavel", scene()};
        require(first.culprit != second.culprit, "hidden-state counterexample differs");
        OfferOwner first_owner(1);
        OfferOwner second_owner(2);
        const auto first_offer = first_owner.reconcile(first.permitted);
        const auto second_offer = second_owner.reconcile(second.permitted);
        for (const bool help : {false, true}) {
            for (const auto locale : {Locale::English, Locale::French}) {
                const auto first_view = render(*first_offer, {help, locale, PlayerFluency::Novice});
                const auto second_view = render(*second_offer, {help, locale, PlayerFluency::Novice});
                require(first_view.choices == second_view.choices, "hidden culprit cannot affect count, order, state, text, tooltip, or accessibility");
                require(first_view.exact_terms == second_view.exact_terms && first_view.profile == second_view.profile,
                        "identical permitted terms/profile");
                require(first_view.choices[0].action == Action::RequestLedgerInspection &&
                        first_view.choices[1].action == Action::OfferRelief && first_view.choices[2].action == Action::Leave,
                        "authored order is not secret-ranked");
                require(!first_view.choices[1].enabled && first_view.choices[1].tooltip.starts_with("Marin has not agreed"),
                        "disabled cause derives only from permitted evidence");
            }
        }
    }

    void permitted_feedback() {
        DraftStore store;
        const auto id = store.add("fire ring", Rule::EnglishCompound);
        require(id.has_value(), "feedback fixture setup");
        const DraftSelection selected{SelectionKind::Draft, *id};
        const auto reasons = store.cast(selected, {}, {true, true, true, false, false}).reasons;
        require(reasons == std::vector{Feedback::CharacterKnowledge, Feedback::MissingMechanics, Feedback::Permission, Feedback::Resources},
                "knowledge, mechanics, permission, and resources are separate permitted channels");
        require(store.cast(selected, {}, {}).reasons == std::vector{Feedback::Undisclosed}, "undisclosed causes cannot leak through precedence");
        require(store.cast(selected, all_knowledge(), {false, false, false, true, true}).reasons == std::vector{Feedback::Undisclosed},
                "paid and permitted still cannot fabricate mechanics or successful cast");
    }

    std::string command(PracticeSession& session, std::string_view input) {
        std::ostringstream output;
        require(session.command(input, output), "session remains active");
        return output.str();
    }

    void practice() {
        PracticeSession session;
        std::string before_transfer;
        before_transfer += command(session, "commands");
        before_transfer += command(session, "discover");
        before_transfer += command(session, "fluency fluent");
        before_transfer += command(session, "help on");
        before_transfer += command(session, "respond fire ring | ring | no guaranteed opening");
        before_transfer += command(session, "respond mur de feu | mur | wall");
        require(session.stage() == PracticeStage::PredictFrench, "early transfer probe does not expose answer or advance");
        before_transfer += command(session, "respond cercle de feu | cercle | no casting mechanics");
        before_transfer += command(session, "retain 3");
        before_transfer += command(session, "inspect");
        require(session.stage() == PracticeStage::RepairOrder, "inspection reaches repair");
        before_transfer += command(session, "repair mur de feu");
        before_transfer += command(session, "inspect");
        before_transfer += command(session, "repair cercle feu");
        before_transfer += command(session, "cast draft 6");
        before_transfer += command(session, "repair cercle de feu");
        before_transfer += command(session, "repair cercle de feu");
        require(session.stage() == PracticeStage::Transfer, "two repaired constructions reach transfer");
        before_transfer += command(session, "transfer");
        before_transfer += command(session, "draft french mur de feu");
        before_transfer += command(session, "inspect");
        require(before_transfer.find("mur de feu") == std::string::npos && before_transfer.find("[mur [de feu]]") == std::string::npos,
                "fresh transfer phrase and bracket hidden until response, including probes");
        const auto revealed = command(session, "respond mur de feu | mur | wall replaces circular; fire stays; cannot cast");
        require(revealed.find("[mur [de feu]]") != std::string::npos && session.stage() == PracticeStage::Complete,
                "fresh answer revealed only after recorded response");
        require(session.responses().back().original == "mur de feu | mur | wall replaces circular; fire stays; cannot cast",
                "prediction is retained verbatim");
        require(session.responses().back().help && session.responses().back().reported_fluency == PlayerFluency::Fluent,
                "assistance and prior familiarity retained separately");
        require(session.character_knowledge() == Knowledge{}, "correct response and help do not acquire knowledge");
        const auto mixed = command(session, "draft mixed feu-ring");
        require(mixed.find("game convention, not French") != std::string::npos, "free practice supports reviewed mixed form honestly");
        const auto feedback = command(session, "feedback");
        require(feedback.find("not a live quote") != std::string::npos && feedback.find("cannot be paid") != std::string::npos,
                "permission/resource cards are not simulated runtime results");
        std::istringstream input(std::string(MaxInputBytes + 1, 'x') + "\ndiscover\nquit\n");
        std::ostringstream output;
        require(run_practice(input, output) == 0, "stream consumer returns cleanly");
        require(output.str().find("discarded without changing state") != std::string::npos &&
                output.str().find("Discover fire") != std::string::npos, "oversized line drains without executing a prefix");
        std::istringstream eof("discover");
        std::ostringstream eof_output;
        require(run_practice(eof, eof_output) == 0 && eof_output.str().find("Discover fire") != std::string::npos, "final non-newline command");
    }
}

int main() {
    try {
        fragments();
        input_rejections();
        lifetime();
        no_fallback();
        disclosure();
        offers();
        two_worlds();
        permitted_feedback();
        practice();
        std::cout << "Language cases passed.\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
