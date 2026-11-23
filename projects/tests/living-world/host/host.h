#pragma once

#include <acquisition/observation.h>
#include <magic/runtime.h>

#include <expected>
#include <memory>

namespace CE::LivingWorld {
    class Scene;
    class Recovery;
    class Host final {
    public:
        class Access {
            std::shared_ptr<const void> owner_;
            EntityRef principal_;
            friend class Host;
            friend class Recovery;
        public:
            Access() = default;
        };
        class Accepted {
            std::shared_ptr<const void> owner_;
            WorldScope scope_;
            std::uint64_t id_ = 0;
            friend class Host;
        public:
            Accepted() = default;
        };
        class CastQuote {
            std::shared_ptr<const void> owner_;
            HealBody body_;
            friend class Host;
        public:
            const HealBody& body() const noexcept { return body_; }
        };
        struct AcceptanceFailure {
            Error error = Error::InvalidBody;
            std::optional<::Magic::Diagnostic> diagnostic;
            bool unsupported = false;
        };

    private:
        ::Magic::Catalog catalog_;
        ::Magic::PlanHandle plan_;
        std::shared_ptr<const void> authority_;
        std::shared_ptr<const void> lineage_;
        std::unique_ptr<Detail::Image> image_;
        std::unique_ptr<Detail::Image> staging_;
        std::uint64_t issued_ = 0;
        std::optional<std::size_t> queued_;
        bool reserved_ = false;
        bool presenting_ = false;

        class Reservation {
            Host* host_ = nullptr;
            friend class Host;
            explicit Reservation(Host& host) noexcept;
        public:
            Reservation(Reservation&& other) noexcept;
            Reservation(const Reservation&) = delete;
            Reservation& operator=(const Reservation&) = delete;
            ~Reservation() noexcept;
            void finalize() noexcept;
        };
        friend class Scene;
        friend class Recovery;
        friend struct HostTestAccess;
        static std::expected<OwnerFence, Error> allocate_fence() noexcept;
        bool authorized(const Access& access) const noexcept;
        std::optional<std::size_t> actor_index(EntityRef ref) const noexcept;
        ReadSet reads(EntityRef caster, EntityRef recipient) const noexcept;
        static std::optional<PlanBinding> binding(const ::Magic::Catalog&, const ::Magic::PlanHandle&,
                                                  std::uint64_t id) noexcept;
        static const Detail::AcceptedPlan* find_plan(const Detail::Image&, const PlanBinding&) noexcept;
        const Detail::AcceptedPlan* resolve(const Access&, const Accepted&) const noexcept;
        std::expected<Action, Error> issue(const HealBody&);
        bool live(const HealBody& body) const noexcept;
        std::expected<Reservation, Error> reserve(const Access&, const Action&, OwnerFence);
        static bool canonical(const HealBody&) noexcept;
        void publish() noexcept;

    public:
        Host(::Magic::Catalog catalog, ::Magic::PlanHandle plan,
             WorldScope scope = {}, std::uint32_t cap = 20, std::uint32_t price = 3);
        Host(const Host&) = delete;
        Host& operator=(const Host&) = delete;
        ~Host() = default;

        Access grant(EntityRef principal) const;
        EntityRef entity(std::uint64_t id) const noexcept;
        OwnerFence fence() const noexcept;
        std::expected<Actor, Error> actor(const Access&, EntityRef) const noexcept;
        std::expected<Action, Error> action(const Access&, EntityRef recipient);
        std::expected<Accepted, AcceptanceFailure> accept(const Access&, std::string_view source) noexcept;
        std::expected<::Magic::PlanHandle, Error> inspect(const Access&, const Accepted&) const noexcept;
        std::expected<CastQuote, Error> quote(const Access&, const Accepted&, EntityRef recipient) const noexcept;
        std::expected<Action, Error> action(const Access&, const CastQuote&);
        std::expected<Action, Error> action(const Access&, EntityRef recipient, const Accepted&);
        std::expected<Receipt, Error> lookup(const Access&, const Action&) const noexcept;
        std::expected<Receipt, Error> queue(const Access&, const Action&, OwnerFence);
        std::expected<Receipt, Error> settle(const Access&, const Action&) noexcept;
        Delivery deliver() noexcept;
        std::expected<ActionDelivery, Error> delivery(const Access&, const Action&) const noexcept;
        std::expected<Page, Error> read(const Access&, std::optional<Cursor> = {}) const noexcept;

        std::expected<void, Error> edit_actor(Actor actor);
        std::expected<void, Error> set_price(std::uint32_t price);
        std::uint32_t price() const noexcept;
        std::expected<void, Error> set_schema(std::uint64_t schema);
    };
}
