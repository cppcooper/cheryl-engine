#pragma once

#include "payoff.h"
#include <istream>

namespace OfflineStory {
    enum class PlayPhase { Bargain, Payoff, Complete };
    struct PlayChoice {
        PlayPhase phase;
        D::Selection selection;
    };
    enum class PlayRefusal { Unknown, Phase, Owner };
    struct PlayError {
        PlayRefusal kind;
        std::optional<D::SessionError> owner;
    };
    struct PlayProbe; // Test-only friend; no host/debug command is exposed by the terminal protocol.
    class PlayDriver {
        SelectedBargainHandle selected_;
        BargainEncounter bargain_;
        PayoffHandle payoff_content_;
        std::optional<PayoffEncounter> payoff_;
        D::View offer_;
        PlayPhase phase_ = PlayPhase::Bargain;
        std::array<std::uint64_t, 2> preparation_effects_;
        PlayDriver(SelectedBargainHandle, BargainEncounter, D::View, std::array<std::uint64_t, 2>);
        friend struct PlayProbe;
    public:
        PlayDriver(PlayDriver&&) noexcept = default;
        PlayDriver& operator=(PlayDriver&&) noexcept = default;
        static std::expected<PlayDriver, Error> start(std::uint64_t);
        const D::View& offer() const noexcept { return offer_; }
        PlayPhase phase() const noexcept { return phase_; }
        const SelectedBargainHandle& selected() const noexcept { return selected_; }
        std::string_view speaker() const noexcept;
        std::expected<PlayChoice, PlayError> resolve(std::string_view) const;
        std::expected<void, PlayError> apply(const PlayChoice&);
        std::expected<void, D::SessionError> refresh();
        std::expected<void, Error> advance(); // Only a real completed bargain can prepare its one payoff.
        std::expected<void, Error> presence(bool);
        std::string render(std::size_t frame_bytes = 16384) const; // Bounded, pure presentation of the retained offer/state.
    };
    struct PlayLimits {
        std::size_t commands = 64, line_bytes = 256, output_bytes = 65536, frame_bytes = 16384;
    };
    inline constexpr PlayLimits maximum_play_limits{};
    enum class PlayExit { Complete = 0, Early = 1, Usage = 2, Limit = 3, Io = 4, Failure = 5 };
    std::string_view play_help() noexcept;
    PlayExit run_play(PlayDriver&, std::istream&, std::ostream&, std::ostream&, const PlayLimits& = {});
    int play(std::uint64_t, std::istream&, std::ostream&, std::ostream&);
}
