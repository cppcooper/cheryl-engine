#pragma once
#ifndef PROCGEN_ENGINE_MEMORY
#error "Background weather requires the actual Engine memory path"
#endif
#include "runtime-admission.h"
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace ProcGen {
    // Private single simulation-owner/single producer protocol. Not a job system.
    // Control/adoption and each individual Pin require caller synchronization.
    // Separate live pins may be sampled/released by permitted readers. Hot
    // metadata operations try-lock/refuse busy; Pin release takes a short mutex
    // section containing only fixed metadata, never solver/destruction work.
    class BackgroundWeather {
    public:
        using Clock = std::chrono::steady_clock;
        static constexpr std::size_t slot_count = 3, pin_limit = 4;
        enum class Code { ok, busy, full, pins, foreign, stale, order, duplicate, current, closed, budget, profile, solver, exception };
        struct Token {
            Index stream = 0, epoch = 0, generation = 0, request = 0, sequence = 0, from_tick = 0, to_tick = 0;
            std::size_t slot = 0;
            bool operator==(const Token&) const = default;
        };
        struct Ticket { Index stream, epoch, request; };
        struct Simulation {
            Token cursor;
            Index intervals = 0;
            double infiltration_m3 = 0, overflow_m3 = 0;
        };
        struct Sample {
            Token token;
            Index age_ticks;
            double publication_age_ms, surface_kelvin, east_m_s, north_m_s, vapour_mm, soil_mm;
            bool old_epoch;
        };
        struct Adoption { Token previous; Index intervals; };
        struct Timing {
            Token token;
            double solver_ms = 0, reclaim_ms = 0;
            Index work = 0;
            CE::Memory::Detail::ThreadAllocationCounts allocations;
        };
        struct Status {
            Code last = Code::ok;
            Index epoch = 0, reclaimed = 0, published = 0, discarded = 0, work_left = 0, updates_left = 0;
            std::size_t pins = 0, occupied = 0;
            bool busy = false, closing = false, done = false;
            RegionalFailure failure{FailureKind::invalid_input, RegionalReason::weather, 0, {}};
            std::array<char, 192> exception_text{};
            std::array<Timing, 16> timings{};
            std::size_t timing_count = 0;
            std::thread::id producer, last_reclaimer;
        };
        // Optional deterministic test/measurement gates; invoked on producer with
        // no protocol lock held. Hooks must not call join or throw.
        struct Hooks {
            void* context = nullptr;
            void (*before_compute)(void*, const Token&) noexcept = nullptr;
            void (*after_compute)(void*, const Token&) noexcept = nullptr;
            void (*before_reclaim)(void*, const Token&) noexcept = nullptr;
        };
    private:
        struct Frame {
            RegionalState state;
            std::optional<RegionalReport> report;
            Token token;
            Clock::time_point published;
        };
        enum class Phase { free, building, published, retired, reclaiming };
        struct Slot {
            std::optional<Frame> frame;
            std::atomic<std::size_t> pins{0};
            Phase phase = Phase::free;
            Token token;
            Index generation = 0;
        };
        struct Job { Token token; std::size_t input; };
        std::array<Slot, slot_count> slots_;
        std::optional<RegionalState> restore_;
        std::optional<Job> job_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::atomic<Index> epoch_{1};
        std::atomic<std::size_t> pins_{0};
        Index stream_, next_request_ = 0;
        std::size_t head_ = 0;
        Token adopted_;
        Status status_;
        Hooks hooks_;
        std::thread thread_;
        bool busy_ = false, closing_ = false, ready_ = false, done_ = false;

        static bool profile(const RegionalState&);
        Code validate(const Token&) const;
        std::size_t free_slot() const;
        bool reclaimable() const;
        void release(std::size_t) noexcept;
        void run() noexcept;
        void reclaim(std::size_t);
        void timing(Timing);
    public:
        // Borrowing pins must be released before the owner is destroyed. close()
        // refuses held pins without joining; release them, then close again.
        class Pin {
            friend class BackgroundWeather;
            BackgroundWeather* owner_ = nullptr;
            const Frame* frame_ = nullptr;
            Token token_;
            Pin(BackgroundWeather* owner, const Frame* frame, Token token) noexcept
            : owner_(owner), frame_(frame), token_(token) {}
        public:
            Pin() = default;
            Pin(const Pin&) = delete;
            Pin& operator=(const Pin&) = delete;
            Pin(Pin&& other) noexcept;
            Pin& operator=(Pin&& other) noexcept;
            ~Pin() { reset(); }
            void reset() noexcept;
            [[nodiscard]] Token token() const noexcept { return token_; }
            [[nodiscard]] const RegionalState& state() const; // borrowed, tests/inspection off the hot path
        };
        explicit BackgroundWeather(RegionalState&& initial);
        BackgroundWeather(RegionalState&& initial, Hooks hooks);
        ~BackgroundWeather();
        BackgroundWeather(const BackgroundWeather&) = delete;
        BackgroundWeather& operator=(const BackgroundWeather&) = delete;
        [[nodiscard]] std::expected<Pin, Code> pin_latest();
        [[nodiscard]] std::expected<Pin, Code> pin(Token);
        [[nodiscard]] std::expected<Ticket, Code> request(Token expected_head, Index ticks = 1);
        [[nodiscard]] std::expected<Adoption, Code> adopt(const Pin&, Simulation&);
        [[nodiscard]] Code retire(Token);
        [[nodiscard]] std::expected<Sample, Code> sample(const Pin&, std::size_t cell, Index model_now) const;
        [[nodiscard]] Code restore(RegionalState&&); // explicit new timeline; one pending replacement
        [[nodiscard]] Code close(); // control operation; may join after all pins release
        [[nodiscard]] Status status() const; // diagnostic/control path, not a foreground hot call
        [[nodiscard]] bool wait_idle(std::chrono::milliseconds watchdog);
        [[nodiscard]] bool wait_reclaimed(Index count, std::chrono::milliseconds watchdog);
    };
}
