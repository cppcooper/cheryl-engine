#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace CE::LivingWorld::Language {
    inline constexpr std::size_t MaxInputBytes = 256;

    enum class Rule { EnglishNoun, EnglishCompound, FrenchForm, MixedEnglish };
    enum class AnalysisStatus { Incomplete, InvalidConstruction, UnsupportedInput, InvalidUtf8, Oversized };
    enum class Origin { English, French, GameConvention };
    enum class Role { Content, Head, Modifier, Link, Join, Uncomposed };
    enum class Form { Unspecified, CircularArrangement, Wall };
    enum class Relation { Unspecified, SelectedFireMaterial, FieryMaterialAppearance };
    enum class Gate {
        EnglishFire, EnglishRing, EnglishCompound, FrenchCircle, FrenchFire, FrenchConstruction,
        FrenchWall, Coinage, FrenchMotif, MixedJoin, Count
    };
    using Knowledge = std::array<bool, static_cast<std::size_t>(Gate::Count)>;

    struct ByteSpan {
        std::size_t begin{};
        std::size_t end{};
        bool operator==(const ByteSpan&) const = default;
    };

    struct Token {
        ByteSpan span;
        std::string surface;
        Origin origin{};
        Role role{};
        std::string sense;
        std::vector<std::string> claims;
        Gate gate{};
        bool operator==(const Token&) const = default;
    };

    struct FragmentPatch {
        Form form{Form::Unspecified};
        Relation relation{Relation::Unspecified};
        bool fire{true};
        bool operator==(const FragmentPatch&) const = default;
    };

    enum class Slot { Form, Dimensions, Magnitude, Target, Anchor, Delivery, Trigger, Duration, Price, ConstructionRepair };

    struct Fragment {
        std::string original_utf8;
        std::size_t submitted_bytes{};
        Rule rule{};
        AnalysisStatus status{AnalysisStatus::UnsupportedInput};
        std::vector<Token> tokens;
        std::optional<std::size_t> head;
        std::optional<std::size_t> modifier;
        std::optional<std::size_t> link;
        std::optional<FragmentPatch> patch;
        std::vector<std::string> construction_claims;
        std::vector<Gate> dependencies;
        std::vector<Slot> unresolved;
        std::string language_label;
        std::string bracket;
        std::string gloss;
        bool operator==(const Fragment&) const = default;
        bool castable() const { return false; }
    };

    bool valid_utf8(std::string_view input);
    Fragment analyze(std::string_view input, Rule selected_rule);
    bool knows(const Knowledge& knowledge, Gate gate);
    bool knows_all(const Knowledge& knowledge, const std::vector<Gate>& dependencies);
    std::string_view slot_name(Slot slot);
}
