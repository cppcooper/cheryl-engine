#pragma once

#include <cheryl/export.h>
#include <core/diagnostics.h>
#include <core/rendering/graphics-domain.h>
#include <core/resources/allocators.h>
#include <core/resources/objects/object-reservation.hpp>
#include <internals/exceptions.h>

#include "block.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace CE::Rendering::Resources {
    struct Provider;
}

namespace CE::Assets::Cache {
    /** Synchronized cache counters; entries count strong map owners, not all external handles. */
    struct Stats {
        CE::Diagnostics::DomainId domain = 0; ///< Stable diagnostic cache identity.
        std::uint64_t entries = 0; ///< Keys currently retained by this cache.
        std::uint64_t publications = 0; ///< New-key publication count over this cache lifetime.
        std::uint64_t replacements = 0; ///< Replacements of previously present keys.
    };

    /** Guards the single active provider/loading-owner domain shared by all caches.
     * This is publication/teardown context, not an eviction or residency manager.
     * Cache maps retain strong handles until explicit clear/replacement or provider
     * teardown; readers retain independent handles across either operation.
     * Distinct owned Loaders still share this domain. Teardown rejects publication;
     * after release, a later provider may bind it. Old handles keep their old native
     * domain and cannot migrate to the replacement provider.
     */
    class Context {
        friend struct CE::Rendering::Resources::Provider;
        static CE_API std::mutex provider_mutex_;
        static CE_API const CE::Rendering::Resources::Provider* bound_provider_;
        static CE_API std::thread::id owner_;
        static CE_API bool releasing_;
        static CE_API CE::Rendering::GraphicsDomain bound_domain_;

    public:
        static CE_API void verify_provider(const CE::Rendering::Resources::Provider& provider);
        /** Evict the bound provider's GPU-backed caches after matching native invalidation.
         * Call on the loading/graphics owner. Stale, foreign and uncommitted records
         * are ignored; CPU FontService catalogues and external handles survive.
         * Runtime installs this consumer on its Context bus. Standalone hosts may
         * register it themselves. Native cleanup never depends on this listener.
         */
        static CE_API bool evict_graphics_domain(const CE::Rendering::GraphicsDomainStateChanged& change) noexcept;
        [[nodiscard]] static bool is_bound_to(const CE::Rendering::Resources::Provider& provider) noexcept {
            std::lock_guard lock(provider_mutex_);
            return bound_provider_ == &provider;
        }

    protected:
        static CE_API void bind_provider(const CE::Rendering::Resources::Provider& provider);

    private:
        static bool begin_provider_release(const CE::Rendering::Resources::Provider& provider) noexcept {
            std::lock_guard lock(provider_mutex_);
            if (bound_provider_ != &provider || releasing_)
                return false;
            releasing_ = true;
            return true;
        }
        static void release_provider(const CE::Rendering::Resources::Provider* provider) noexcept {
            std::lock_guard lock(provider_mutex_);
            if (bound_provider_ != provider)
                return;
            bound_provider_ = nullptr;
            bound_domain_ = {};
            owner_ = {};
            releasing_ = false;
        }
        static CE_API void
        verify_locked(const CE::Rendering::Resources::Provider& provider, CE::Rendering::GraphicsDomainSnapshot snapshot);
        static CE_API void clear_provider_assets() noexcept;
    };

    /**
     * Caches constructed assets by key. reserve() provides storage whose slots
     * callers construct selectively with emplace(); the older allocate()
     * interface returns unconstructed handles for manual construction.
     * Publish complete assets under a unique lock; readers copy retained handles
     * under a shared lock. Construction and final release happen outside the lock.
     */
    template <
        typename AssetType,
        typename Key = std::filesystem::path,
        typename Allocator = std::allocator<std::pair<const Key, std::shared_ptr<AssetType>>>>
    struct Store : Context {
        /** Independent retained asset owner copied under the cache lock. */
        using Handle = std::shared_ptr<AssetType>;
        /** Exact unordered-map identity; normalization/aliases belong to a derived manager. */
        using key_type = Key;

    protected:
        mutable std::shared_mutex assets_mutex_; ///< Protects map/counters; user construction/final release occurs outside it.
        std::unordered_map<Key, Handle, std::hash<Key>, std::equal_to<Key>, Allocator> loaded_assets{}; ///< Strong key-to-generation owners; direct access requires the corresponding lock.
        const CE::Diagnostics::DomainId domain_ = CE::Diagnostics::next_domain_id(); ///< Identity of this cache facade and its counters.
        std::uint64_t publications_ = 0; ///< New-key count protected by assets_mutex_.
        std::uint64_t replacements_ = 0; ///< Existing-key replacement count protected by assets_mutex_.

    public:
        Store() = default;
        virtual ~Store() { clear_assets(); }
        [[nodiscard]] virtual Handle get_asset(const Key& key) const {
            std::shared_lock lock(assets_mutex_);
            if (const auto asset = loaded_assets.find(key); asset != loaded_assets.end()) {
                return asset->second;
            }
            return nullptr;
        }
        [[nodiscard]] bool contains(const Key& key) const {
            std::shared_lock lock(assets_mutex_);
            return loaded_assets.contains(key);
        }
        [[nodiscard]] std::size_t size() const {
            std::shared_lock lock(assets_mutex_);
            return loaded_assets.size();
        }
        [[nodiscard]] Stats diagnostics() const {
            std::shared_lock lock(assets_mutex_);
            return {domain_, loaded_assets.size(), publications_, replacements_};
        }
        virtual void clear_assets() noexcept {
            // Preserve allocator identity: swapping maps with unequal, nonpropagating
            // allocators is invalid. Default production caches still use std::allocator.
            const auto allocator = [this] {
                std::shared_lock lock(assets_mutex_);
                return loaded_assets.get_allocator();
            }();
            decltype(loaded_assets) retired(allocator);
            {
                std::unique_lock lock(assets_mutex_);
                retired.swap(loaded_assets);
            }
            // A deleter can inspect this cache without re-entering its write lock.
        }

    protected:
        explicit Store(const Allocator& allocator)
        : loaded_assets(allocator) {}

        Handle publish_asset(const Key& key, Handle asset) {
            return publish_asset(key, std::move(asset), []() noexcept {});
        }

        // A derived cache can commit prepared metadata in the publication lock.
        // The callback must not throw or reenter this cache.
        template <typename Published> Handle publish_asset(const Key& key, Handle asset, Published&& published) {
            static_assert(std::is_nothrow_invocable_v<Published>);
            if (!asset)
                throw Exceptions::failed_operation(CE_HERE, "Cannot publish an empty asset");
            std::unique_lock lock(assets_mutex_);
            // Keep this local owner until the lock has unwound. Node/rehash
            // failure may destroy an insertion candidate while still inside
            // try_emplace; that must not run its final deleter under this lock.
            const auto [entry, inserted] = loaded_assets.try_emplace(key, asset);
            if (inserted)
                ++publications_;
            std::forward<Published>(published)();
            return entry->second;
        }
        Handle replace_asset(const Key& key, Handle asset) {
            if (!asset)
                throw Exceptions::failed_operation(CE_HERE, "Cannot publish an empty asset");
            Handle retired;
            Handle result;
            {
                std::unique_lock lock(assets_mutex_);
                const auto [entry, inserted] = loaded_assets.try_emplace(key, Handle{});
                if (inserted)
                    ++publications_;
                else
                    ++replacements_;
                retired = std::exchange(entry->second, std::move(asset));
                result = entry->second;
            }
            return result;
        }
        /** Reserve raw slots for selective construction with emplace(). */
        template <typename Derived> auto reserve(const std::size_t N) {
            static_assert(std::is_base_of_v<AssetType, Derived>);
            return CE::Memory::Objects::Reservation<Derived, CE::Memory::Allocators::ObjectPoolAllocator<Derived>>(N);
        }

        /** Provide raw object handles for callers that construct slots manually. */
        template <typename Derived> std::vector<std::shared_ptr<Derived>> allocate(const std::size_t N) {
            static_assert(std::is_base_of_v<AssetType, Derived>, "The allocated class type must be derived from the managed type.");
            if (N == 0) {
                return {};
            }
            using A_OPA = std::allocator_traits<CE::Memory::Allocators::ObjectPoolAllocator<Derived>>;
            CE::Memory::Allocators::ObjectPoolAllocator<Derived> allocator;
            auto context = allocator.context();
            // Allocate one raw batch, then hand out independent per-slot
            // handles; the pool context outlives this temporary allocator.
            auto raw = A_OPA::allocate(allocator, N);
            std::shared_ptr<Derived> owner(raw, [](void* p) {});
            CE::Memory::Block<Derived> block{owner, owner, CE::Memory::Pointers::calculate_alignment(raw), N};
            // Manual callers must construct slots themselves. Their final
            // handle destroys its slot and returns exactly that slot to pool.
            return block.vector([context](Derived* p) noexcept {
                context->construction_context()->destroy(p);
                context->release_owned(p, 1);
            });
        }
    };
}

/**
 * @fn void CE::Assets::Cache::Context::verify_provider(const CE::Rendering::Resources::Provider& provider)
 * @brief Check compatibility with the active global provider/loading owner under its mutex.
 * @param provider Candidate domain.
 * Throws failed_operation for another bound provider, a foreign loading thread or teardown. An unbound context validates without binding; no resource/cache entry is created.
 */

/**
 * @fn bool CE::Assets::Cache::Context::is_bound_to(const CE::Rendering::Resources::Provider& provider) noexcept
 * @return Whether the exact provider address is currently bound, observed under the domain mutex. This observation does not pin its lifetime or authorize concurrent teardown/publication.
 */

/**
 * @fn void CE::Assets::Cache::Context::bind_provider(const CE::Rendering::Resources::Provider& provider)
 * @brief Verify the global domain and, if absent, bind it to this provider/current loading thread.
 * @param provider Borrowed provider retained by the surrounding runtime until loading is quiescent.
 * Conflicting provider/owner or teardown throws failed_operation; distinct Loader objects do not create independent domains.
 */

/**
 * @fn template<typename AssetType, typename Key, typename Allocator> CE::Assets::Cache::Store<AssetType, Key, Allocator>::Store()
 * @brief Create an empty synchronized strong-handle cache with its own diagnostic identity; global provider binding is deferred until a derived loading operation.
 */

/**
 * @fn template<typename AssetType, typename Key, typename Allocator> CE::Assets::Cache::Store<AssetType, Key, Allocator>::Store(const Allocator& allocator)
 * @brief Create an empty cache using the supplied map allocator identity.
 * @param allocator Copied allocator retained by the map; clear_assets() preserves this identity.
 */

/**
 * @fn template<typename AssetType, typename Key, typename Allocator> CE::Assets::Cache::Store<AssetType, Key, Allocator>::~Store()
 * @brief Clear this cache outside final-deleter lock scopes. Quiesce users before facade destruction; independently retained handles may outlive it.
 */

/**
 * @fn template<typename AssetType, typename Key, typename Allocator> typename CE::Assets::Cache::Store<AssetType, Key, Allocator>::Handle CE::Assets::Cache::Store<AssetType, Key, Allocator>::get_asset(const Key& key) const
 * @brief Copy an exact-key strong owner under a shared lock.
 * @param key Stable exact map key; base lookup performs no normalization, load or fallback.
 * @return Retained handle or null when missing, independent of later replacement/clear.
 * Derived managers may define explicit alias policies; native use retains backend-domain limits.
 */

/**
 * @fn template<typename AssetType, typename Key, typename Allocator> bool CE::Assets::Cache::Store<AssetType, Key, Allocator>::contains(const Key& key) const
 * @brief Test exact-key presence under a shared lock without retaining/loading the asset.
 * @param key Exact key; no derived get_asset() fallback is invoked.
 * @return Presence at this observation, not a reservation against later changes.
 */

/**
 * @fn template<typename AssetType, typename Key, typename Allocator> std::size_t CE::Assets::Cache::Store<AssetType, Key, Allocator>::size() const
 * @return Copied current key count under a shared lock, not external owner/resource residency count.
 */

/**
 * @fn template<typename AssetType, typename Key, typename Allocator> CE::Assets::Cache::Stats CE::Assets::Cache::Store<AssetType, Key, Allocator>::diagnostics() const
 * @return Consistent copied map/count statistics under a shared lock, without logging/native calls.
 */

/**
 * @fn template<typename AssetType, typename Key, typename Allocator> void CE::Assets::Cache::Store<AssetType, Key, Allocator>::clear_assets() noexcept
 * @brief Detach every map entry under its write lock, then release retired handles after unlocking.
 * Preserves allocator identity and cumulative publication/replacement counters. Independent readers retain old assets. Because the operation is noexcept, allocator/locking failures cannot escape and terminate rather than report partial success.
 */

/**
 * @fn template<typename AssetType, typename Key, typename Allocator> typename CE::Assets::Cache::Store<AssetType, Key, Allocator>::Handle CE::Assets::Cache::Store<AssetType, Key, Allocator>::publish_asset(const Key& key, Handle asset)
 * @brief Publish a nonnull complete candidate only if the exact key is absent.
 * @param key Exact publication key.
 * @param asset Retained candidate built before taking the cache write lock.
 * @return Existing winner or newly published handle. Null input throws failed_operation; allocation failure leaves existing entries unchanged and final candidate destruction outside the write lock.
 */

/**
 * @fn template<typename AssetType, typename Key, typename Allocator> template<typename Published> typename CE::Assets::Cache::Store<AssetType, Key, Allocator>::Handle CE::Assets::Cache::Store<AssetType, Key, Allocator>::publish_asset(const Key& key, Handle asset, Published&& published)
 * @brief Publish-or-observe a key, then commit prepared derived metadata while holding the write lock.
 * @tparam Published Statically nothrow-invocable callback; it must not reenter this cache.
 * @param key Exact identity.
 * @param asset Nonnull completed candidate.
 * @param published Invoked after successful map lookup/insertion, including when an entry already existed.
 * @return Existing or newly inserted retained owner. Candidate construction and final release remain outside the lock; allocation failure occurs before the metadata callback.
 */

/**
 * @fn template<typename AssetType, typename Key, typename Allocator> typename CE::Assets::Cache::Store<AssetType, Key, Allocator>::Handle CE::Assets::Cache::Store<AssetType, Key, Allocator>::replace_asset(const Key& key, Handle asset)
 * @brief Atomically replace/insert one exact key under the cache write lock.
 * @param key Exact identity.
 * @param asset Nonnull completed candidate built before publication.
 * @return Retained new owner; old readers retain old generations.
 * Null candidates throw failed_operation; map allocation failure preserves the previous entry. Final retired-owner release occurs after unlocking; no multi-key transaction is provided.
 */

/**
 * @fn template<typename AssetType, typename Key, typename Allocator> template<typename Derived> auto CE::Assets::Cache::Store<AssetType, Key, Allocator>::reserve(std::size_t N)
 * @brief Reserve unconstructed pool slots for selective construction with Reservation::emplace().
 * @tparam Derived AssetType-derived object type.
 * @param N Slot count, zero creates an empty reservation.
 * @return Move-only reservation retaining its release context; constructed handles may outlive the cache/reservation. No cache key is published.
 * @see [Reservations](cheryl-guide:resources/resource-lifetime#reservations)
 */

/**
 * @fn template<typename AssetType, typename Key, typename Allocator> template<typename Derived> auto CE::Assets::Cache::Store<AssetType, Key, Allocator>::allocate(std::size_t N) -> std::vector<std::shared_ptr<Derived>>
 * @brief Legacy raw-slot allocation for callers that explicitly construct each object.
 * @tparam Derived AssetType-derived class.
 * @param N Requested slot count; zero returns an empty vector.
 * @return Independent per-slot handles retaining pool release context; this helper does not construct objects or publish cache entries.
 * Callers must use the compatible construction tracker before dereferencing live objects. Final release destroys a tracked live slot and returns exactly that slot; allocation failures propagate. Prefer reserve()/emplace() for safe selective construction.
 */
