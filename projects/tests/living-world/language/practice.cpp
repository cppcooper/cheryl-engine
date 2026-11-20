#include "practice.hpp"

#include <charconv>
#include <istream>
#include <ostream>
#include <system_error>
#include <utility>

namespace CE::LivingWorld::Language {
    namespace {
        std::optional<std::uint64_t> number(std::string_view input) {
            std::uint64_t result{};
            const auto converted = std::from_chars(input.data(), input.data() + input.size(), result);
            if (converted.ec != std::errc{} || converted.ptr != input.data() + input.size() || result == 0) {
                return std::nullopt;
            }
            return result;
        }

        void print_explanation(const Explanation& explanation, std::ostream& output) {
            if (explanation.language_label) {
                output << *explanation.language_label << '\n';
            }
            for (const auto& token : explanation.tokens) {
                output << '[' << token.span.begin << ',' << token.span.end << ") " << token.surface;
                if (token.sense) {
                    output << ": " << *token.sense;
                }
                output << '\n';
            }
            if (explanation.analysis) {
                output << *explanation.analysis << '\n';
            }
            if (explanation.gloss) {
                output << *explanation.gloss << '\n';
            }
            if (explanation.repair && explanation.repair != explanation.gloss) {
                output << *explanation.repair << '\n';
            }
            if (!explanation.missing_mechanics.empty()) {
                output << "Still unspecified:";
                for (const auto& slot : explanation.missing_mechanics) {
                    output << ' ' << slot;
                }
                output << ".\n";
            }
            if (explanation.status) {
                output << *explanation.status << '\n';
            }
        }

        std::optional<Rule> selected_rule(std::string_view input) {
            if (input == "noun") { return Rule::EnglishNoun; }
            if (input == "english") { return Rule::EnglishCompound; }
            if (input == "french") { return Rule::FrenchForm; }
            if (input == "mixed") { return Rule::MixedEnglish; }
            return std::nullopt;
        }
    }

    PracticeSession::PracticeSession() {
        access_.content = true;
        access_.original = true;
        access_.origins = true;
        access_.status = true;
        auto teaching = [](FieldAccess& field) {
            field.disclosed = true;
            field.assistance.fill(true);
        };
        for (auto& sense : access_.senses) {
            teaching(sense);
        }
        teaching(access_.analysis);
        teaching(access_.gloss);
        teaching(access_.mechanics);
        teaching(access_.repair);
    }

    void PracticeSession::inspect(std::ostream& output) const {
        const auto selection = drafts_.selected();
        if (!selection) {
            output << "No selected draft. Begin with discover.\n";
            return;
        }
        const auto* draft = drafts_.find(*selection);
        if (draft) {
            print_explanation(project(draft->fragment, character_, access_, display_.help), output);
        }
    }

    void PracticeSession::respond(std::string_view response, std::ostream& output) {
        const bool transfer = stage_ == PracticeStage::Transfer && transfer_prompted_;
        if (stage_ != PracticeStage::PredictEnglish && stage_ != PracticeStage::PredictFrench && !transfer) {
            output << "No prediction is awaiting a response.\n";
            return;
        }
        const auto divider = response.find(" | ");
        const auto notes = divider == std::string_view::npos ? divider : response.find(" | ", divider + 3);
        if (divider == std::string_view::npos || notes == std::string_view::npos || divider == 0 || notes == divider + 3 ||
            notes + 3 == response.size()) {
            output << "Use respond <phrase> | <head> | <mechanics prediction>.\n";
            return;
        }
        if (responses_.size() >= DraftStore::Capacity) {
            output << "Practice response capacity reached. End this finite session.\n";
            return;
        }
        const auto phrase = response.substr(0, divider);
        const auto head = response.substr(divider + 3, notes - divider - 3);
        if (!transfer && phrase == "mur de feu") {
            output << "That content is reserved for the fresh transfer response. No readback was opened.\n";
            return;
        }
        const bool english = stage_ == PracticeStage::PredictEnglish;
        const auto rule = english ? Rule::EnglishCompound : Rule::FrenchForm;
        const bool correct = phrase == (english ? "fire ring" : transfer ? "mur de feu" : "cercle de feu") &&
                             head == (english ? "ring" : transfer ? "mur" : "cercle");
        const auto id = drafts_.add(phrase, rule);
        if (!id) {
            output << "Draft capacity reached; no selection or cast was substituted.\n";
            return;
        }
        responses_.push_back({stage_, std::string(response), stage_help_used_ || display_.help, display_.player_fluency, correct});
        stage_help_used_ = false;
        output << (correct ? "Phrase and head match this lesson. " : "Phrase/head do not match the reviewed answer. ")
               << "Mechanics prediction is recorded, not automatically graded. No character knowledge was acquired.\n"
               << "Selected draft D" << *id << ".\n";
        if (transfer) {
            output << "Response recorded before reveal: [mur [de feu]]. Wall of fire. Head mur replaces cercle; "
                      "fire and the fiery-material-appearance relation stay. Dimensions and other mechanics remain unresolved. "
                      "No cast occurred. Session complete; this is not evidence of learning or fun.\n";
            stage_ = PracticeStage::Complete;
        } else if (english) {
            output << "Reviewed answer: fire ring; head ring; no measured opening is specified.\n"
                      "Now predict the taught French circular motif, its head, and whether it can cast.\n";
            stage_ = PracticeStage::PredictFrench;
        } else {
            output << "Reviewed answer: cercle de feu; head cercle; still uncastable. Use inspect to compare, then repair.\n";
            stage_ = PracticeStage::Inspect;
        }
    }

    void PracticeSession::repair(std::string_view phrase, std::ostream& output) {
        if (stage_ != PracticeStage::RepairOrder && stage_ != PracticeStage::RepairLink) {
            output << "Inspect the worked pair before repairing.\n";
            return;
        }
        if (phrase == "mur de feu") {
            output << "That content is reserved for the fresh transfer response. No readback was opened.\n";
            return;
        }
        const auto id = drafts_.add(phrase, Rule::FrenchForm);
        if (!id) {
            output << "Draft capacity reached; no prior inscription was selected.\n";
            return;
        }
        if (responses_.size() < DraftStore::Capacity) {
            responses_.push_back({stage_, std::string(phrase), stage_help_used_ || display_.help,
                                  display_.player_fluency, phrase == "cercle de feu"});
        }
        output << "Selected new draft D" << *id << ". Earlier retained inscriptions are unchanged and were not selected.\n";
        if (phrase != "cercle de feu") {
            output << "This does not repair the selected taught construction. No cast occurred.\n";
            inspect(output);
            return;
        }
        stage_help_used_ = false;
        output << "Repair accepted for this lesson: head cercle, then de feu. Motif only; no supported mechanics.\n";
        if (stage_ == PracticeStage::RepairOrder) {
            stage_ = PracticeStage::RepairLink;
            const auto broken = drafts_.add("cercle feu", Rule::FrenchForm);
            output << "Next failed draft";
            if (broken) { output << " D" << *broken; }
            output << ": cercle feu. Repair the missing linker using repair <phrase>.\n";
        } else {
            stage_ = PracticeStage::Transfer;
            output << "Both repairs recorded. Use transfer for a fresh head-word card; its answer remains hidden.\n";
        }
    }

    bool PracticeSession::command(std::string_view line, std::ostream& output) {
        if (line == "quit") {
            return false;
        }
        if (line.size() > MaxInputBytes || !valid_utf8(line)) {
            output << "Command rejected: maximum 256 UTF-8 bytes. No state changed.\n";
            return true;
        }
        if (line == "commands") {
            output << "discover; respond <phrase> | <head> | <mechanics prediction>; inspect; repair <phrase>; transfer; "
                      "help on/off; locale en/fr; fluency novice/fluent/unreported; status; "
                      "retain <draft-id>; select draft/prior <id>; cast draft/prior <id>; "
                      "draft noun/english/french/mixed <phrase> (after lesson); feedback; quit.\n";
        } else if (line == "discover" && stage_ == PracticeStage::Discover) {
            const auto id = drafts_.add("fire", Rule::EnglishNoun);
            if (!id) {
                output << "Draft capacity reached.\n";
                return true;
            }
            output << "Discover fire: English noun, flames/burning, not shoot. Fire content; form unspecified. "
                      "Uncastable draft; no cast occurred.\n"
                      "Cards: English ring = circular arrangement. A preceding fire noun modifies head ring; "
                      "its fire-material relation is a declared game coinage. French cercle = circle; feu = fire. "
                      "The taught French rule puts the form first, followed by de feu.\n"
                      "Predict before readback: add circular organization in English; name its head and whether "
                      "a 100 mm opening is guaranteed. Use respond <phrase> | <head> | <mechanics prediction>.\n";
            stage_ = PracticeStage::PredictEnglish;
        } else if (line.starts_with("respond ")) {
            respond(line.substr(8), output);
        } else if (line == "inspect") {
            inspect(output);
            if (stage_ == PracticeStage::Inspect) {
                for (const auto& example : {analyze("fire ring", Rule::EnglishCompound), analyze("cercle de feu", Rule::FrenchForm),
                                            analyze("feu-ring", Rule::MixedEnglish)}) {
                    print_explanation(project(example, character_, access_, display_.help), output);
                }
                const auto id = drafts_.add("feu cercle", Rule::FrenchForm);
                if (id) {
                    stage_ = PracticeStage::RepairOrder;
                    output << "Repair draft D" << *id << ": feu cercle. It has no composed patch. "
                              "Use repair <phrase> under the selected French construction.\n";
                }
            }
        } else if (line.starts_with("repair ")) {
            repair(line.substr(7), output);
        } else if (line == "transfer" && stage_ == PracticeStage::Transfer) {
            transfer_prompted_ = true;
            output << "Fresh card: French mur is a noun meaning wall. Using the practiced French construction, "
                      "write a fiery wall motif, name its head, and predict what changes and whether it can cast. "
                      "The completed phrase is hidden until your response.\n";
        } else if (line == "help on" || line == "help off") {
            display_.help = line == "help on";
            stage_help_used_ = stage_help_used_ || display_.help;
            output << "Display help " << (display_.help ? "on" : "off") << "; character knowledge unchanged.\n";
        } else if (line == "locale en" || line == "locale fr") {
            display_.locale = line == "locale en" ? Locale::English : Locale::French;
            output << "Interface locale selected; reviewed English fallback retained where no localized wording exists.\n";
        } else if (line == "fluency novice" || line == "fluency fluent" || line == "fluency unreported") {
            display_.player_fluency = line == "fluency fluent" ? PlayerFluency::Fluent
                                      : line == "fluency novice" ? PlayerFluency::Novice : PlayerFluency::Unreported;
            output << "Player familiarity recorded separately; no character knowledge or social admission changed.\n";
        } else if (line == "status") {
            const auto selection = drafts_.selected();
            output << "Responses recorded: " << responses_.size() << ". Character lesson knowledge: not acquired. ";
            if (selection) {
                output << "Selected " << (selection->kind == SelectionKind::Draft ? 'D' : 'A') << selection->id << '.';
            } else {
                output << "No selection.";
            }
            output << " Social profiles are 5/15/40/75/100, not fluency or help levels.\n";
        } else if (line.starts_with("retain ")) {
            const auto draft = number(line.substr(7));
            const auto retained = draft ? drafts_.retain_practice_inscription(*draft) : std::nullopt;
            if (retained) {
                output << "Retained immutable practice inscription A" << *retained
                       << "; not selected, not a physical spellbook, and still uncastable.\n";
            } else {
                output << "Cannot retain that draft; no prior inscription changed.\n";
            }
        } else if (line.starts_with("select ") || line.starts_with("cast ")) {
            const bool casting = line.starts_with("cast ");
            const auto arguments = line.substr(casting ? 5 : 7);
            const auto divider = arguments.find(' ');
            const auto kind = arguments.substr(0, divider);
            const auto id = divider == std::string_view::npos ? std::nullopt : number(arguments.substr(divider + 1));
            if (!id || (kind != "draft" && kind != "prior")) {
                output << "Use an explicit draft/prior identity. No cast occurred.\n";
                return true;
            }
            const DraftSelection requested{kind == "draft" ? SelectionKind::Draft : SelectionKind::PriorInscription, *id};
            if (casting) {
                const auto refused = drafts_.cast(requested, character_, {true, true, true, std::nullopt, std::nullopt});
                for (const auto reason : refused.reasons) {
                    output << feedback_text(reason) << '\n';
                }
                output << "Earlier inscriptions were not cast instead. No debit, release, or effect exists in this leaf.\n";
            } else {
                output << (drafts_.select(requested) ? "Identity selected deliberately.\n" : "Selection refused and cleared.\n");
            }
        } else if (line.starts_with("draft ") && stage_ == PracticeStage::Complete) {
            const auto arguments = line.substr(6);
            const auto divider = arguments.find(' ');
            const auto rule = selected_rule(arguments.substr(0, divider));
            if (!rule || divider == std::string_view::npos) {
                output << "Specify noun/english/french/mixed and exact phrase bytes.\n";
            } else {
                const auto id = drafts_.add(arguments.substr(divider + 1), *rule);
                if (id) {
                    output << "Selected D" << *id << ".\n";
                    inspect(output);
                } else {
                    output << "Draft capacity reached; no fallback.\n";
                }
            }
        } else if (line == "feedback") {
            output << "Diagnosis cards, not a live quote or cast:\n";
            for (const auto reason : {Feedback::Syntax, Feedback::CharacterKnowledge, Feedback::MissingMechanics,
                                      Feedback::Permission, Feedback::Resources}) {
                output << feedback_text(reason) << '\n';
            }
        } else {
            output << "Command unavailable at this lesson stage; use commands. No answer or state was substituted.\n";
        }
        return true;
    }

    int run_practice(std::istream& input, std::ostream& output) {
        PracticeSession session;
        output << "Cheryl fragment practice. No casting engine or character acquisition is connected. "
                  "Admitted lesson; interface help and player familiarity are separate from character knowledge. "
                  "Start with discover; commands lists controls.\n";
        std::string line;
        bool oversized = false;
        char byte{};
        while (input.get(byte)) {
            if (byte != '\n') {
                if (line.size() < MaxInputBytes) {
                    line.push_back(byte);
                } else {
                    oversized = true;
                }
                continue;
            }
            if (oversized) {
                output << "Command exceeds 256 bytes; discarded without changing state.\n";
            } else if (!session.command(line, output)) {
                return 0;
            }
            line.clear();
            oversized = false;
        }
        if (input.bad()) {
            output << "Input stream failed. No cast occurred.\n";
            return 1;
        }
        if (oversized) {
            output << "Command exceeds 256 bytes; discarded without changing state.\n";
        } else if (!line.empty()) {
            static_cast<void>(session.command(line, output));
        }
        return 0;
    }
}
