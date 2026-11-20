#include "fragment.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace CE::LivingWorld::Language {
    bool valid_utf8(std::string_view input) {
        std::size_t offset = 0;
        while (offset < input.size()) {
            const auto lead = static_cast<unsigned char>(input[offset]);
            if (lead <= 0x7f) {
                ++offset;
                continue;
            }
            std::size_t length = 0;
            std::uint32_t codepoint = 0;
            std::uint32_t minimum = 0;
            if (lead >= 0xc2 && lead <= 0xdf) {
                length = 2;
                codepoint = lead & 0x1f;
                minimum = 0x80;
            } else if (lead >= 0xe0 && lead <= 0xef) {
                length = 3;
                codepoint = lead & 0x0f;
                minimum = 0x800;
            } else if (lead >= 0xf0 && lead <= 0xf4) {
                length = 4;
                codepoint = lead & 0x07;
                minimum = 0x10000;
            } else {
                return false;
            }
            if (input.size() - offset < length) {
                return false;
            }
            for (std::size_t continuation = 1; continuation < length; ++continuation) {
                const auto byte = static_cast<unsigned char>(input[offset + continuation]);
                if ((byte & 0xc0) != 0x80) {
                    return false;
                }
                codepoint = (codepoint << 6) | (byte & 0x3f);
            }
            if (codepoint < minimum || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
                return false;
            }
            offset += length;
        }
        return true;
    }

    namespace {
        Token word(std::string_view surface, std::size_t begin, Origin origin, Role role,
                   std::string sense, Gate gate, std::vector<std::string> claims) {
            return {{begin, begin + surface.size()}, std::string(surface), origin, role, std::move(sense), std::move(claims), gate};
        }

        void mechanics(Fragment& fragment) {
            fragment.unresolved = {Slot::Dimensions, Slot::Magnitude, Slot::Target, Slot::Anchor,
                                   Slot::Delivery, Slot::Trigger, Slot::Duration, Slot::Price};
        }
    }

    Fragment analyze(std::string_view input, Rule selected_rule) {
        Fragment fragment;
        fragment.submitted_bytes = input.size();
        fragment.rule = selected_rule;
        if (input.size() > MaxInputBytes) {
            fragment.status = AnalysisStatus::Oversized;
            return fragment;
        }
        fragment.original_utf8 = input;
        if (!valid_utf8(input)) {
            fragment.status = AnalysisStatus::InvalidUtf8;
            return fragment;
        }
        const bool bare = input == "fire" && selected_rule == Rule::EnglishNoun;
        const bool english = input == "fire ring" && selected_rule == Rule::EnglishCompound;
        const bool mixed = input == "feu-ring" && selected_rule == Rule::MixedEnglish;
        const bool circle = input == "cercle de feu" && selected_rule == Rule::FrenchForm;
        const bool wall = input == "mur de feu" && selected_rule == Rule::FrenchForm;
        const bool reversed = input == "feu cercle" && selected_rule == Rule::FrenchForm;
        const bool missing = input == "cercle feu" && selected_rule == Rule::FrenchForm;
        if (!(bare || english || mixed || circle || wall || reversed || missing)) {
            return fragment;
        }
        mechanics(fragment);
        fragment.status = AnalysisStatus::Incomplete;
        if (bare || english) {
            fragment.tokens.push_back(word("fire", 0, Origin::English, bare ? Role::Content : Role::Modifier,
                                           "noun.flames", Gate::EnglishFire, {"EN-L1"}));
            fragment.dependencies = {Gate::EnglishFire};
            fragment.head = 0;
            fragment.language_label = "English";
            fragment.construction_claims = {"EN-L1"};
        }
        if (bare) {
            fragment.patch = FragmentPatch{};
            fragment.unresolved.insert(fragment.unresolved.begin(), Slot::Form);
            fragment.bracket = "[fire]";
            fragment.gloss = "Fire; form unspecified.";
        } else if (english || mixed) {
            if (mixed) {
                fragment.tokens.push_back(word("feu", 0, Origin::French, Role::Modifier,
                                               "noun.combustion", Gate::FrenchFire, {"FR-L3"}));
                fragment.tokens.push_back(word("-", 3, Origin::GameConvention, Role::Join,
                                               "mixed-join-marker", Gate::MixedJoin, {"EN-G1/G2"}));
            }
            fragment.tokens.push_back(word("ring", mixed ? 4 : 5, Origin::English, Role::Head,
                                           "noun.circular-arrangement", Gate::EnglishRing, {"EN-L2"}));
            fragment.head = fragment.tokens.size() - 1;
            fragment.modifier = 0;
            if (mixed) {
                fragment.link = 1;
            }
            fragment.dependencies = {mixed ? Gate::FrenchFire : Gate::EnglishFire, Gate::EnglishRing,
                                      Gate::EnglishCompound, Gate::Coinage};
            if (mixed) {
                fragment.dependencies.push_back(Gate::MixedJoin);
            }
            fragment.patch = FragmentPatch{Form::CircularArrangement, Relation::SelectedFireMaterial};
            fragment.construction_claims = {"EN-G1/G2"};
            fragment.language_label = mixed ? "Mixed origins; English-governed game convention, not French"
                                            : "English; declared game coinage";
            fragment.bracket = mixed ? "[[feu]-ring]" : "[[fire] ring]";
            fragment.gloss = mixed ? "The lesson's circular fire motif using French feu and English ring; the mixed join is host-authored."
                                   : "A circular arrangement of fire in this lesson; no numeric hole is specified.";
        } else {
            fragment.construction_claims = {"FR-G1", "FR-G2"};
            fragment.language_label = "French";
            if (circle || wall) {
                fragment.tokens.push_back(word(wall ? "mur" : "cercle", 0, Origin::French, Role::Head,
                                               wall ? "noun.wall" : "noun.circle-or-circular-arrangement",
                                               wall ? Gate::FrenchWall : Gate::FrenchCircle,
                                               {wall ? "FR-L4" : "FR-L2/L2b"}));
                fragment.tokens.push_back(word("de", wall ? 4 : 7, Origin::French, Role::Link,
                                               "link.within-specific-de-feu-construction", Gate::FrenchConstruction,
                                               {"FR-G1", "FR-G2"}));
                fragment.tokens.push_back(word("feu", wall ? 7 : 10, Origin::French, Role::Modifier,
                                               "noun.combustion", Gate::FrenchFire, {"FR-L3"}));
                fragment.head = 0;
                fragment.modifier = 2;
                fragment.link = 1;
                fragment.dependencies = {wall ? Gate::FrenchWall : Gate::FrenchCircle, Gate::FrenchFire,
                                          Gate::FrenchConstruction, Gate::FrenchMotif};
                fragment.patch = FragmentPatch{wall ? Form::Wall : Form::CircularArrangement, Relation::FieryMaterialAppearance};
                if (circle) {
                    fragment.construction_claims.push_back("FR-C3");
                }
                fragment.bracket = wall ? "[mur [de feu]]" : "[cercle [de feu]]";
                fragment.gloss = wall ? "Wall of fire; a fiery wall motif. This draft has no supported wall mechanics."
                                      : "Circle of fire; a circular fiery motif, not an exact magical annulus.";
            } else {
                fragment.status = AnalysisStatus::InvalidConstruction;
                auto circle_token = word("cercle", reversed ? 4 : 0, Origin::French, Role::Uncomposed,
                                         "noun.circle-or-circular-arrangement", Gate::FrenchCircle, {"FR-L2/L2b"});
                auto fire_token = word("feu", reversed ? 0 : 7, Origin::French, Role::Uncomposed,
                                       "noun.combustion", Gate::FrenchFire, {"FR-L3"});
                if (reversed) {
                    fragment.tokens = {std::move(fire_token), std::move(circle_token)};
                } else {
                    fragment.tokens = {std::move(circle_token), std::move(fire_token)};
                }
                fragment.dependencies = {Gate::FrenchCircle, Gate::FrenchFire, Gate::FrenchConstruction};
                fragment.unresolved.insert(fragment.unresolved.begin(), Slot::ConstructionRepair);
                fragment.language_label = "French-origin words; selected construction not satisfied";
                fragment.bracket = "No accepted grouping for this taught construction.";
                fragment.gloss = reversed ? "For this lesson, put the form first and connect fire with de: cercle de feu."
                                          : "For this taught construction the linker is missing: cercle de feu. Do not silently insert de.";
            }
        }
        return fragment;
    }

    bool knows(const Knowledge& knowledge, Gate gate) {
        const auto index = static_cast<std::size_t>(gate);
        return index < knowledge.size() && knowledge[index];
    }

    bool knows_all(const Knowledge& knowledge, const std::vector<Gate>& dependencies) {
        return std::ranges::all_of(dependencies, [&](Gate gate) { return knows(knowledge, gate); });
    }

    std::string_view slot_name(Slot slot) {
        switch (slot) {
            case Slot::Form: return "form";
            case Slot::Dimensions: return "dimensions";
            case Slot::Magnitude: return "magnitude";
            case Slot::Target: return "target";
            case Slot::Anchor: return "anchor";
            case Slot::Delivery: return "delivery";
            case Slot::Trigger: return "trigger";
            case Slot::Duration: return "duration";
            case Slot::Price: return "price";
            case Slot::ConstructionRepair: return "construction repair";
        }
        return "unspecified";
    }
}
