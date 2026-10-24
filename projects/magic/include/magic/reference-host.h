#pragma once

#include <magic/compiler.h>

#include <array>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <utility>

namespace Magic::Reference {
    inline constexpr std::size_t actor_capacity = 16;
    inline constexpr std::size_t pending_capacity = 8;
    inline constexpr std::size_t item_capacity = 8;
    inline constexpr std::size_t touch_capacity = 16;

    enum class Error {
        InvalidLimits,
        InvalidCatalog,
        InvalidActor,
        ActorLimit,
        MissingActor,
        GenerationLimit,
        RevisionLimit,
        NoPlan,
        PlanLimit,
        UnsupportedEntry,
        UnsupportedDelivery,
        UnsupportedBinding,
        SchemaMismatch,
        Permission,
        Contact,
        Resources,
        InvalidAim,
        EmptySelection,
        Overlap,
        ActiveSlow,
        StatusLimit,
        PendingLimit,
        SelectorLimit,
        TargetLimit,
        WorkLimit,
        TimeLimit,
        ActionLimit,
        TicketLimit,
        InvalidTicket,
        ForeignTicket,
        StaleTicket,
        ConsumedTicket,
        MissingAction,
        ItemLimit,
        MissingItem,
        DisabledItem,
        SpentItem,
        InvalidEvent,
        ForeignEvent,
        RepeatedEvent,
        DuplicateFact,
        MissingFact,
        EventLimit,
        NonceLimit,
        InvalidCheckpoint,
        IncompatibleCheckpoint
    };
    struct Failure {
        Error code;
        std::uint64_t action = 0; // advance context for an accepted action; failed releases allocate no action (zero)
        std::uint64_t tick = 0;   // failed advance's attempted event tick; world clock remains unchanged
    };
    struct Point {
        std::int32_t x = 0;
        std::int32_t y = 0;
        bool operator==(const Point&) const = default;
    };
    struct ActorId {
        // Logical reference within one host. The opaque Ticket, rather than an ID, authorizes release.
        std::uint32_t slot = 0;
        std::uint64_t generation = 0;
        bool operator==(const ActorId&) const = default;
    };
    struct ActorSpec {
        Point position;
        std::uint32_t health = 100;
        std::uint32_t max_health = 100;
        std::uint32_t mana = 0;
        std::uint32_t base_speed_mm_per_s = 1000;
        bool eligible = true;
        bool willing = true;
        bool can_cast = false;
        bool operator==(const ActorSpec&) const = default;
    };
    struct Slow {
        std::uint32_t permille = 1000;
        std::uint64_t until_tick = 0; // active on [activation, until_tick); zero means absent
        bool operator==(const Slow&) const = default;
    };
    struct ActorView {
        ActorId id;
        bool present = false;
        ActorSpec data;
        Slow slow;
        // Exact thousandths of mm/s; speed has no position integration side effect.
        [[nodiscard]] std::uint64_t speed_milli_mm_per_s() const noexcept {
            return static_cast<std::uint64_t>(data.base_speed_mm_per_s) * slow.permille;
        }
        bool operator==(const ActorView&) const = default;
    };
    struct PendingView {
        bool present = false;
        std::uint64_t action = 0; // monotonic logical identity, scoped to this host
        std::uint64_t due_tick = 0;
        ActorId caster;
        Point aim; // accepted world-mm point in G0, independent of camera and caster movement
        std::size_t plan_bytes = 0;
        bool operator==(const PendingView&) const = default;
    };
    // Observational projection only: it omits retained plans, schemas, items and live authority.
    struct Snapshot {
        std::uint64_t tick = 0;
        std::uint64_t revision = 0;
        std::uint64_t actions_issued = 0;
        std::array<ActorView, actor_capacity> actors;
        std::array<PendingView, pending_capacity> pending;
        bool operator==(const Snapshot&) const = default;
    };
    struct Request {
        ActorId caster;
        std::optional<ActorId> contact;
        std::optional<Point> aim;
    };
    struct HostLimits {
        // Only tighten these ceilings, including zero; metrics bound operations rather than CPU cycles/RSS.
        std::size_t actors = actor_capacity;
        std::size_t pending = pending_capacity;
        std::size_t statuses = actor_capacity;
        std::size_t selected_targets = 128; // (binding, actor) pairs over the complete operation
        std::size_t selector_work = 1024;   // visited slots, including empty slots
        std::size_t work = 16384;
        std::size_t plan_bytes = 32768; // per retained M1 plan, logical metric; handles avoid deep copies
        std::uint64_t max_tick = 1'000'000'000;
        std::uint64_t max_action = 1'000'000;
        std::uint64_t max_ticket = 1'000'000;
        std::uint64_t max_revision = 1'000'000'000;
        std::uint64_t max_generation = 1'000'000;
        std::size_t items = item_capacity;
        std::size_t touch_receipts = touch_capacity; // lifetime admissions; never evicted
        std::uint64_t max_event_nonce = 1'000'000;
        std::uint64_t max_fact_id = 1'000'000'000;
        bool operator==(const HostLimits&) const = default;
    };
    struct ItemId {
        std::uint32_t slot = 0;
        std::uint64_t generation = 0;
        bool operator==(const ItemId&) const = default;
    };
    struct ItemView {
        ItemId id;
        bool present = false;
        bool enabled = true;
        std::uint32_t charges = 0;
        std::uint32_t prepaid_mana = 0; // informational installation cost; never refundable
        std::size_t plan_bytes = 0;
        bool operator==(const ItemView&) const = default;
    };
    struct TouchRecord {
        std::uint64_t fact = 0;
        std::uint64_t nonce = 0;
        ItemId item;
        ActorId toucher;
        bool consumed = false;
        bool operator==(const TouchRecord&) const = default;
    };
    struct ItemSnapshot {
        std::array<ItemView, item_capacity> items;
        std::array<TouchRecord, touch_capacity> ledger;
        std::uint64_t events_issued = 0;
        bool operator==(const ItemSnapshot&) const = default;
    };
    struct Usage {
        std::size_t work = 0;
        std::size_t selector_work = 0;
        std::size_t selected_targets = 0;
    };
    struct Preview {
        std::uint32_t mana_cost = 0;
        std::uint64_t due_tick = 0;
        Usage usage;
    };
    enum class EventKind { Applied, Queued, Activated, Cancelled };
    struct Event {
        EventKind kind = EventKind::Applied;
        std::uint64_t action = 0;
        std::uint64_t tick = 0;
    };
    struct Observation {
        Snapshot before;
        Snapshot after;
        std::array<Event, pending_capacity> events;
        std::size_t event_count = 0;
        Usage usage;
    };
    using Observed = std::shared_ptr<const Observation>;
    using StepResult = std::expected<Observed, Failure>;

    struct ItemObservation {
        Snapshot before, after;
        ItemSnapshot items_before, items_after;
        Usage usage;
    };
    using ItemObserved = std::shared_ptr<const ItemObservation>;
    class ReferenceHost;
    // Opaque native-owner admission. Copies share one use; failure preserves it. Never serialized/revived.
    class TouchEvent {
        std::shared_ptr<const void> owner_;
        std::uint64_t nonce_ = 0;
        TouchEvent(std::shared_ptr<const void> owner, std::uint64_t nonce)
        : owner_(std::move(owner)), nonce_(nonce) {}
        friend class ReferenceHost;
    public:
        TouchEvent() = default;
    };
    // Owned immutable continuation, distinct from observational Snapshot. No live tickets are retained.
    class Checkpoint {
        struct Data;
        std::shared_ptr<const Data> data_;
        explicit Checkpoint(std::shared_ptr<const Data> data)
        : data_(std::move(data)) {}
        friend class ReferenceHost;
    public:
        Checkpoint() = default;
    };
    // Opaque host-incarnation-bound input. Copying does not create a second use.
    // Default is invalid; destruction does not cancel an already queued action.
    class Ticket {
        std::shared_ptr<const void> owner_;
        std::uint64_t serial_ = 0;
        Ticket(std::shared_ptr<const void> owner, std::uint64_t serial)
        : owner_(std::move(owner)), serial_(serial) {}
        friend class ReferenceHost;

    public:
        Ticket() = default;
    };

    // Private single-thread reference owner. No callbacks, wall clock, external effects or persistence.
    // Owner edits are trusted fixture operations. Invalid/range/resource errors return Failure;
    // allocation exceptions propagate before adoption and preserve the existing host/ticket.
    // Preview is pure. Prepare supersedes the old ticket; release checks live state then staleness.
    // Successful release pays 10 mana/effect once; cancellation/activation failure does not refund.
    // Any failed advance preserves the entire world/clock/queue; explicit correction/cancel is required.
    // Observations own fixed snapshots; retain their const handle across later operations/destruction.
    class ReferenceHost final {
        struct State {
            Snapshot world;
            std::array<PlanHandle, pending_capacity> plans;
            ItemSnapshot items;
            std::array<PlanHandle, item_capacity> inscriptions;
        };
        struct Prepared {
            PlanHandle plan;
            Request request;
            std::uint64_t revision;
            std::uint64_t serial;
        };
        friend class Checkpoint;
        Catalog catalog_;
        HostLimits limits_;
        State state_;
        std::shared_ptr<const void> owner_;
        std::uint64_t tickets_issued_ = 0;
        std::optional<Prepared> prepared_;
        ReferenceHost(Catalog catalog, HostLimits limits);
        struct Meter;
        void validate(const PlanHandle& plan, const Request& request, const State& state, Meter& meter, bool paying) const;
        void payload_schema(const PlanHandle& plan, Meter& meter) const;
        void inscription(const PlanHandle& plan, Meter& meter) const;
        void apply(const PlanHandle& plan, std::optional<ActorId> recipient, std::optional<Point> aim, State& state, Meter& meter) const;
        void continuation(const State& state, std::uint64_t tickets, Meter& meter) const;
        [[nodiscard]] const Prepared& authorize(const Ticket& ticket) const;
        [[nodiscard]] StepResult adopt(State&& next, Observation observation);

    public:
        ReferenceHost(const ReferenceHost&) = delete;
        ReferenceHost& operator=(const ReferenceHost&) = delete;
        [[nodiscard]] static std::expected<std::unique_ptr<ReferenceHost>, Failure> create(const Catalog& catalog, HostLimits limits = {});
        [[nodiscard]] Snapshot snapshot() const noexcept { return state_.world; }
        [[nodiscard]] ItemSnapshot items() const noexcept { return state_.items; }
        // Trusted owner installation prepays one charge; removal/disable/spending never refunds it.
        [[nodiscard]] std::expected<ItemId, Failure> install(const PlanHandle& plan, ActorId funder);
        [[nodiscard]] std::expected<void, Failure> enable_item(ItemId item, bool enabled);
        [[nodiscard]] std::expected<void, Failure> remove_item(ItemId item);
        // Only this trusted native-owner boundary admits facts. Logical IDs alone cannot apply payloads.
        [[nodiscard]] std::expected<TouchEvent, Failure> admit_touch(std::uint64_t fact, ItemId item, ActorId toucher);
        // Owner reauthorizes only an existing unconsumed fact (e.g. after restore); no new logical admission.
        [[nodiscard]] std::expected<TouchEvent, Failure> resume_touch(std::uint64_t fact) const;
        // Failed applications preserve admission, charge, resources and all world state.
        [[nodiscard]] std::expected<ItemObserved, Failure> touch(const TouchEvent& event);
        // Capture may refuse incoherent retained work (departed caster, changed schema, time horizon or work cap).
        [[nodiscard]] std::expected<Checkpoint, Failure> checkpoint() const;
        // Exact schema/limit identity, whole-world rewind, fresh incarnation; failed restore changes nothing.
        [[nodiscard]] std::expected<void, Failure> restore(const Checkpoint& checkpoint);
        [[nodiscard]] std::expected<ActorId, Failure> add_actor(const ActorSpec& actor);
        [[nodiscard]] std::expected<void, Failure> update_actor(ActorId id, const ActorSpec& actor);
        [[nodiscard]] std::expected<void, Failure> remove_actor(ActorId id);
        [[nodiscard]] std::expected<void, Failure> replace_catalog(const Catalog& catalog);
        [[nodiscard]] std::expected<Preview, Failure> preview(const PlanHandle& plan, const Request& request) const;
        [[nodiscard]] std::expected<Ticket, Failure> prepare(const PlanHandle& plan, const Request& request);
        [[nodiscard]] std::expected<void, Failure> discard(const Ticket& ticket);
        [[nodiscard]] StepResult release(const Ticket& ticket);
        [[nodiscard]] StepResult advance_to(std::uint64_t tick);
        // Trusted owner cancellation by host-local action ID; caller-supplied IDs confer no casting authority.
        [[nodiscard]] StepResult cancel(std::uint64_t action);
    };
    [[nodiscard]] std::string_view error_name(Error error) noexcept;
}
