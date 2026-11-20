#pragma once

#include "drafts.hpp"
#include "projection.hpp"

#include <iosfwd>

namespace CE::LivingWorld::Language {
    enum class PracticeStage { Discover, PredictEnglish, PredictFrench, Inspect, RepairOrder, RepairLink, Transfer, Complete };
    struct PracticeResponse {
        PracticeStage stage{};
        std::string original;
        bool help{};
        PlayerFluency reported_fluency{};
        bool phrase_and_head_match{};
    };

    class PracticeSession {
        PracticeStage stage_{PracticeStage::Discover};
        DraftStore drafts_;
        Knowledge character_{};
        DisplaySettings display_;
        ExplanationAccess access_;
        std::vector<PracticeResponse> responses_;
        bool transfer_prompted_{};
        bool stage_help_used_{};

        void inspect(std::ostream& output) const;
        void respond(std::string_view response, std::ostream& output);
        void repair(std::string_view phrase, std::ostream& output);

    public:
        PracticeSession();
        bool command(std::string_view line, std::ostream& output);
        PracticeStage stage() const { return stage_; }
        const std::vector<PracticeResponse>& responses() const { return responses_; }
        const Knowledge& character_knowledge() const { return character_; }
    };

    int run_practice(std::istream& input, std::ostream& output);
}
