#pragma once

#include <magic/world-adapter.h>
#include <optional>

namespace Magic::Contact {
    struct Limits {
        std::size_t plan_bytes = 32768, work = 4096, result_bytes = sizeof(Change);
        std::uint64_t tickets = 1'000'000;
    };
    struct Evaluation {
        Change change;
        std::size_t work = 0;
    };
    [[nodiscard]] bool supports(const PlanHandle& plan) noexcept;
    [[nodiscard]] std::uint32_t healed(std::uint32_t health, std::uint32_t cap, std::uint32_t quantity) noexcept;
    // Pure selected interpretation shared by the retained facade and independent owners.
    [[nodiscard]] std::expected<Evaluation, Error> evaluate(
        const PlanHandle& plan, Request request, const WorldAdapter& world, Limits limits = {});
    class Runtime;
    class Ticket {
        std::shared_ptr<const void> incarnation_;
        std::uint64_t serial_ = 0;
        friend class Runtime;
    public:
        Ticket() = default;
    };
    using Result = std::shared_ptr<const Change>;
    class Runtime final {
        struct Prepared {
            PlanHandle plan;
            Request request;
            Facts facts;
            std::uint64_t serial;
        };
        WorldAdapter& world_;
        Limits limits_;
        std::shared_ptr<const void> incarnation_;
        std::uint64_t issued_ = 0;
        std::optional<Prepared> prepared_;
        [[nodiscard]] std::expected<void, Error> authorize(const Ticket& ticket) const noexcept;
    public:
        explicit Runtime(WorldAdapter& world, Limits limits = {});
        Runtime(const Runtime&) = delete;
        Runtime& operator=(const Runtime&) = delete;
        [[nodiscard]] std::expected<Ticket, Error> prepare(const PlanHandle& plan, Request request);
        [[nodiscard]] std::expected<Result, Error> release(const Ticket& ticket);
        [[nodiscard]] std::expected<void, Error> discard(const Ticket& ticket);
    };
}
