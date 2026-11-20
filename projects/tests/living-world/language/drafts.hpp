#pragma once

#include "fragment.hpp"

#include <cstdint>

namespace CE::LivingWorld::Language {
    enum class SelectionKind { Draft, PriorInscription };
    struct DraftSelection {
        SelectionKind kind{};
        std::uint64_t id{};
        bool operator==(const DraftSelection&) const = default;
    };
    struct Draft {
        std::uint64_t id{};
        Fragment fragment;
    };
    enum class Feedback { SelectionRequired, StaleSelection, Syntax, CharacterKnowledge, MissingMechanics, Permission, Resources, Undisclosed };
    struct PermittedFeedback {
        bool syntax{};
        bool character_knowledge{};
        bool mechanics{};
        std::optional<bool> permission;
        std::optional<bool> resources;
    };
    struct CastRefusal {
        DraftSelection requested;
        std::vector<Feedback> reasons;
    };
    std::string_view feedback_text(Feedback feedback);

    class DraftStore {
        std::vector<Draft> drafts_;
        std::vector<Draft> inscriptions_;
        std::optional<DraftSelection> selected_;
        std::uint64_t next_id_{1};

    public:
        static constexpr std::size_t Capacity = 64;
        std::optional<std::uint64_t> add(std::string_view input, Rule rule);
        std::optional<std::uint64_t> retain_practice_inscription(std::uint64_t draft);
        bool select(DraftSelection selection);
        const Draft* find(DraftSelection selection) const;
        std::optional<DraftSelection> selected() const { return selected_; }
        CastRefusal cast(DraftSelection requested, const Knowledge& knowledge, const PermittedFeedback& permitted) const;
    };
}
