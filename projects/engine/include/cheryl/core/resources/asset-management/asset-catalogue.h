#pragma once

#include <core/diagnostics.h>
#include <core/rendering/graphics-domain.h>
#include <core/subsystems/event-bus.h>
#include <internals/exceptions.h>

#include <array>
#include <chrono>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <utility>

namespace CE::Assets::Cache {
    template <typename AssetType, typename Key, typename Allocator> struct Store;

    /** Committed cache operation; a cache hit and source closure are not changes. */
    enum class CatalogueChangeKind {
        Published, ///< An absent exact key acquired a complete generation.
        Replaced,  ///< An existing key acquired a fresh complete generation.
        Reset      ///< Explicitly cleared the entire catalogue, including an empty one.
    };

    /** Generation identity and separately retained native usability, without asset ownership. */
    struct CatalogueGeneration {
        std::uint64_t generation = 0;                  ///< Source-unique increasing identity; gaps for one key are normal.
        CE::Rendering::GraphicsDomain graphics_domain; ///< Empty explicitly means unsupported/unbound provenance.
    };

    /** Owned exact-key observation. AssetType supplies the namespace in the typed channel identity. */
    template <typename AssetType, typename Key> struct AssetCatalogueChanged {
        std::uint64_t source = 0;   ///< Checked permanent catalogue identity, unrelated to native domain identity.
        std::uint64_t revision = 0; ///< Monotonic commit order, including resets.
        CatalogueChangeKind kind = CatalogueChangeKind::Reset; ///< Complete committed cache operation.
        std::optional<Key> key;                                ///< Exact manager key; absent only for Reset.
        std::optional<CatalogueGeneration> previous;           ///< Superseded key generation, if present.
        std::optional<CatalogueGeneration> current;            ///< New key generation; absent for Reset.
        std::chrono::steady_clock::time_point observed_at;     ///< Commit observation on the producer.
        std::uint64_t lost_observations = 0;                   ///< Cumulative bounded-outbox/preparation loss at the attempt.
        std::uint64_t first_lost_revision = 0;                 ///< First lost revision, zero if none.
        std::uint64_t last_lost_revision = 0;                  ///< Latest lost revision; the range need not be contiguous.
    };

    /** Separate exact payload types preserve asset namespaces and key types on a shared bus. */
    template <typename AssetType, typename Key>
    inline const CE::Events::EventChannel<AssetCatalogueChanged<AssetType, Key>> asset_catalogue_changed_event{"asset-catalogue-changed"};

    /** Consistent CPU-only reconciliation state; copying may allocate and never retains assets. */
    template <typename Key> struct CatalogueSnapshot {
        std::uint64_t source = 0;                             ///< Zero for an empty/unsupported source facade.
        std::uint64_t revision = 0;                           ///< Latest committed operation.
        std::unordered_map<Key, CatalogueGeneration> entries; ///< Current exact keys and generation provenance.
        std::uint64_t lost_observations = 0;                  ///< Outbox/preparation losses, independent of bus failure.
        std::uint64_t first_lost_revision = 0;                ///< First lost observation.
        std::uint64_t last_lost_revision = 0;                 ///< Latest lost observation.
        std::size_t pending = 0;                              ///< Retained attempts awaiting a bounded drain.
        bool closed = false;                                  ///< Store destruction closed this source without fabricating Reset.
    };

    /** Watch entry is a coherent seed, followed by strictly newer exact-key changes. */
    template <typename AssetType, typename Key> struct CatalogueObservation {
        std::optional<CatalogueSnapshot<Key>> seed;                  ///< Present only for the synchronous initial callback.
        std::optional<AssetCatalogueChanged<AssetType, Key>> change; ///< Present for later committed changes.
    };

    namespace Detail {
        CE_API void report_catalogue_attempt(
            std::uint64_t source,
            std::uint64_t revision,
            CatalogueChangeKind kind,
            std::uint64_t lost,
            std::size_t pending
        ) noexcept;
    }

    /** Retained catalogue observations, independent of Store/native lifetime.
     * Commits may originate on concurrent producers. One nonrecursive source drain
     * serializes callbacks outside cache locks, attempting at most 64 records per
     * call. Reentrant commits join that drain up to its budget; a later commit or
     * publish_notifications() services residual work. Overflow drops oldest records.
     * Snapshots recover current keys/generations, not retired history. Optional
     * observer failure never changes a successful cache result.
     */
    template <typename AssetType, typename Key> class AssetCatalogue final {
        template <typename, typename, typename> friend struct Store;
        using Change = AssetCatalogueChanged<AssetType, Key>;
        using Observation = CatalogueObservation<AssetType, Key>;
        static constexpr std::size_t capacity = 64;

        struct Pending {
            std::shared_ptr<Change> change;
            CE::Events::EventBus::Publisher destination;
            CE::Events::EventBus::Publisher local;
        };
        struct State {
            mutable std::shared_mutex mutex;
            const std::uint64_t id = CE::Diagnostics::next_domain_id();
            std::uint64_t revision = 0;
            std::unordered_map<Key, CatalogueGeneration> entries;
            CE::Rendering::GraphicsDomain domain;
            CE::Events::EventBus::Publisher publisher;
            bool explicit_route = false;
            std::unique_ptr<CE::Events::EventBus> observers;
            std::size_t watches = 0;
            std::array<Pending, capacity> outbox;
            std::size_t head = 0;
            std::size_t pending = 0;
            std::uint64_t lost = 0;
            std::uint64_t first_lost = 0;
            std::uint64_t last_lost = 0;
            bool draining = false;
            bool closed = false;

            State() {
                if (!id)
                    throw CE::Exceptions::failed_operation(CE_HERE, "Catalogue identity space is exhausted");
            }
            [[nodiscard]] CatalogueSnapshot<Key> snapshot_locked() const {
                return {id, revision, entries, lost, first_lost, last_lost, pending, closed};
            }
            [[nodiscard]] std::uint64_t next_revision() const {
                if (closed || revision == std::numeric_limits<std::uint64_t>::max())
                    throw CE::Exceptions::failed_operation(CE_HERE, "Catalogue is closed or its revision space is exhausted");
                return revision + 1;
            }
            void require_domain() const {
                const auto native = domain.snapshot();
                if (native.id && native.status != CE::Rendering::GraphicsDomainStatus::Available)
                    throw CE::Exceptions::failed_operation(CE_HERE, "Asset publication requires its original available graphics domain");
            }
            void lose(std::uint64_t value) noexcept {
                ++lost;
                if (!first_lost || value < first_lost)
                    first_lost = value;
                if (value > last_lost)
                    last_lost = value;
            }
            [[nodiscard]] bool observing() const noexcept { return !publisher.expired() || watches != 0; }
            std::shared_ptr<Change> prepare(
                CatalogueChangeKind kind,
                const Key* key,
                std::optional<CatalogueGeneration> previous,
                std::optional<CatalogueGeneration> current
            ) const noexcept {
                if (!observing())
                    return {};
                try {
                    return std::make_shared<Change>(Change{
                        id, revision + 1, kind, key ? std::optional<Key>(*key) : std::nullopt, std::move(previous), std::move(current),
                        std::chrono::steady_clock::now()
                    });
                } catch (...) {
                    return {}; // Optional payload allocation cannot reject a cache commit.
                }
            }
            void commit(std::shared_ptr<Change> record) noexcept {
                ++revision; // Capacity was checked before any cache/metadata effects.
                if (!observing())
                    return;
                if (!record) {
                    lose(revision);
                    return;
                }
                if (pending == capacity) {
                    lose(outbox[head].change->revision);
                    outbox[head] = {};
                    head = (head + 1) % capacity;
                    --pending;
                }
                outbox[(head + pending++) % capacity] = {
                    std::move(record), publisher, watches ? observers->publisher() : CE::Events::EventBus::Publisher{}
                };
            }
        };
        std::shared_ptr<State> state_;

        struct WatchState {
            mutable std::recursive_mutex mutex;
            bool active = true;
            bool counted = false;
            std::uint64_t seen = 0;
            std::weak_ptr<State> source;
            std::shared_ptr<std::function<void(const Observation&)>> callback;
            CE::Events::EventBus::Registration registration;

            void reset() noexcept {
                const std::lock_guard lock(mutex);
                active = false;
                if (counted) {
                    if (const auto state = source.lock()) {
                        const std::unique_lock source_lock(state->mutex);
                        --state->watches;
                    }
                    counted = false;
                }
                registration.disconnect();
                callback.reset();
            }
            void deliver(const Change& change) {
                const std::lock_guard lock(mutex);
                if (!active || change.revision <= seen)
                    return;
                const auto state = source.lock();
                if (!state)
                    return;
                {
                    const std::shared_lock source_lock(state->mutex);
                    if (state->closed)
                        return;
                }
                seen = change.revision;
                const auto pinned = callback;
                (*pinned)(Observation{{}, change});
            }
        };

        explicit AssetCatalogue(std::shared_ptr<State> state) noexcept
        : state_(std::move(state)) {}
        [[nodiscard]] static AssetCatalogue create() { return AssetCatalogue(std::make_shared<State>()); }

    public:
        /** Move-only optional listener; reset acknowledges entered callbacks, including safe self-reset. */
        class Watch final {
            friend class AssetCatalogue;
            std::shared_ptr<WatchState> state_;
            explicit Watch(std::shared_ptr<WatchState> state) noexcept
            : state_(std::move(state)) {}
        public:
            Watch() = default;
            Watch(Watch&&) noexcept = default;
            Watch& operator=(Watch&& other) noexcept {
                if (this != &other) {
                    reset();
                    state_ = std::move(other.state_);
                }
                return *this;
            }
            Watch(const Watch&) = delete;
            Watch& operator=(const Watch&) = delete;
            ~Watch() { reset(); }
            void reset() noexcept {
                if (auto state = std::exchange(state_, {}))
                    state->reset();
            }
            [[nodiscard]] bool attached() const {
                if (!state_)
                    return false;
                const std::lock_guard lock(state_->mutex);
                const auto source = state_->source.lock();
                if (!source || !state_->active)
                    return false;
                const std::shared_lock source_lock(source->mutex);
                return !source->closed;
            }
        };

        AssetCatalogue() = default;
        [[nodiscard]] CatalogueSnapshot<Key> snapshot() const {
            if (!state_)
                return {};
            const std::shared_lock lock(state_->mutex);
            return state_->snapshot_locked();
        }
        [[nodiscard]] Watch watch(std::function<void(const Observation&)> callback) const {
            if (!state_ || !callback)
                throw CE::Exceptions::invalid_args(CE_HERE, "Catalogue watch requires a source and callback");
            auto gate = std::make_shared<WatchState>();
            gate->source = state_;
            gate->callback = std::make_shared<std::function<void(const Observation&)>>(std::move(callback));
            Watch result(gate);
            const std::lock_guard gate_lock(gate->mutex);
            CatalogueSnapshot<Key> seed;
            {
                const std::unique_lock lock(state_->mutex);
                if (state_->closed)
                    throw CE::Exceptions::bad_request(CE_HERE, "Catalogue source is closed");
                seed = state_->snapshot_locked();
                gate->seen = seed.revision;
                if (!state_->observers)
                    state_->observers = std::make_unique<CE::Events::EventBus>();
                gate->registration = state_->observers->register_listener(
                    asset_catalogue_changed_event<AssetType, Key>, [weak = std::weak_ptr(gate)](const Change& change) {
                        if (const auto entered = weak.lock())
                            entered->deliver(change);
                    }
                );
                ++state_->watches;
                gate->counted = true;
            }
            // A concurrent drain waits on the gate until the seed has entered.
            const auto pinned = gate->callback;
            (*pinned)(Observation{std::move(seed), {}});
            return result;
        }
        void publish_events_to(CE::Events::EventBus::Publisher publisher) const {
            if (!state_)
                throw CE::Exceptions::bad_request(CE_HERE, "Catalogue source is empty");
            const std::unique_lock lock(state_->mutex);
            if (state_->closed)
                throw CE::Exceptions::bad_request(CE_HERE, "Catalogue source is closed");
            state_->publisher = std::move(publisher);
            state_->explicit_route = true;
        }
        void publish_notifications() const noexcept {
            const auto state = state_;
            if (!state)
                return;
            {
                const std::unique_lock lock(state->mutex);
                if (state->closed || state->draining)
                    return;
                state->draining = true;
            }
            for (std::size_t attempt = 0; attempt < capacity; ++attempt) {
                Pending record;
                std::size_t remaining;
                {
                    const std::unique_lock lock(state->mutex);
                    if (state->closed || !state->pending)
                        break;
                    record = std::exchange(state->outbox[state->head], {});
                    state->head = (state->head + 1) % capacity;
                    remaining = --state->pending;
                    record.change->lost_observations = state->lost;
                    record.change->first_lost_revision = state->first_lost;
                    record.change->last_lost_revision = state->last_lost;
                }
                record.destination.notify(asset_catalogue_changed_event<AssetType, Key>, *record.change);
                record.local.notify(asset_catalogue_changed_event<AssetType, Key>, *record.change);
                Detail::report_catalogue_attempt(
                    state->id, record.change->revision, record.change->kind, record.change->lost_observations, remaining
                );
            }
            const std::unique_lock lock(state->mutex);
            state->draining = false;
        }
    };
}

/** @fn template<typename AssetType, typename Key> auto CE::Assets::Cache::AssetCatalogue<AssetType, Key>::watch(std::function<void(const Observation&)> callback) const
 * Install an owned optional watch and invoke its coherent seed before returning. Subsequent callbacks
 * run on the source drainer, potentially another producer thread, serialized per watch. No cache lock
 * is held. Delayed revisions at or below the seed are filtered. Seed failure propagates and detaches;
 * later failures are isolated by the notification bus. Self-reset is allowed. Settle external users
 * before destroying a Store; a retained source holds CPU metadata and weak notification routes only.
 */
/** @fn template<typename AssetType, typename Key> CE::Assets::Cache::AssetCatalogue<AssetType, Key>::AssetCatalogue()
 * Create an empty source. snapshot() returns source zero; watch/routing reject it and pumping does nothing.
 */
/** @fn template<typename AssetType, typename Key> auto CE::Assets::Cache::AssetCatalogue<AssetType, Key>::snapshot() const
 * Copy coherent exact keys, generations, revision and loss/closure counters under the source lock.
 * Allocation/key-copy failure propagates without changing the catalogue. Domain tokens retain their
 * live native-usability observations, independently of the copied catalogue generation/revision.
 */
/** @fn template<typename AssetType, typename Key> void CE::Assets::Cache::AssetCatalogue<AssetType, Key>::publish_events_to(CE::Events::EventBus::Publisher publisher) const
 * Explicitly override provider routing for future commits, including with an empty destination.
 * Already staged records keep their original destination. This synchronized operation rejects an
 * empty/closed source; it never invokes listeners or borrows the bus/Context.
 */
/** @fn template<typename AssetType, typename Key> void CE::Assets::Cache::AssetCatalogue<AssetType, Key>::publish_notifications() const noexcept
 * Attempt at most 64 ordered staged records outside cache locks. Concurrent/reentrant pumps return
 * without recursively entering listeners. Commits normally pump automatically; call this to service
 * a residual bounded backlog. Each record gets one attempt with no bus-failure retry. Closed/empty
 * sources do nothing. Optional observation failures never alter the cache's direct return/exception.
 */
