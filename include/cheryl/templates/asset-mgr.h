#pragma once
#include <core/resources/allocators.h>
#include <core/resources/objects/object-reservation.hpp>
#include <internals/exceptions.h>

#include "block.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace CE::Assets {
    struct ResourceProvider;

    /** Guards the single active provider/loading-owner domain shared by all caches.
     * This is publication/teardown context, not an eviction or residency manager.
     * Cache maps retain strong handles until explicit clear/replacement or provider
     * teardown; readers retain independent handles across either operation.
     */
    class AssetCacheContext {
        friend struct ResourceProvider;
        inline static std::mutex provider_mutex_;
        inline static const ResourceProvider* bound_provider_ = nullptr;
        inline static std::thread::id owner_;
        inline static bool releasing_ = false;

    public:
        static void verify_provider(const ResourceProvider& provider) {
            std::lock_guard lock(provider_mutex_);
            verify_locked(provider);
        }
        [[nodiscard]] static bool is_bound_to(const ResourceProvider& provider) noexcept {
            std::lock_guard lock(provider_mutex_);
            return bound_provider_ == &provider;
        }

    protected:
        static void bind_provider(const ResourceProvider& provider) {
            std::lock_guard lock(provider_mutex_);
            verify_locked(provider);
            if (!bound_provider_) {
                bound_provider_ = &provider;
                owner_ = std::this_thread::get_id();
            }
        }

    private:
        static bool begin_provider_release(const ResourceProvider& provider) noexcept {
            std::lock_guard lock(provider_mutex_);
            if (bound_provider_ != &provider)
                return false;
            releasing_ = true;
            return true;
        }
        static void release_provider(const ResourceProvider& provider) noexcept {
            std::lock_guard lock(provider_mutex_);
            if (bound_provider_ != &provider)
                return;
            bound_provider_ = nullptr;
            owner_ = {};
            releasing_ = false;
        }
        static void verify_locked(const ResourceProvider& provider) {
            if (bound_provider_ && bound_provider_ != &provider)
                throw Exceptions::failed_operation(CE_HERE, "Asset caches are bound to another resource provider");
            if (bound_provider_ && (releasing_ || owner_ != std::this_thread::get_id()))
                throw Exceptions::failed_operation(CE_HERE, "Asset loading requires the active provider's owner thread");
        }
    };

    /**
     * Caches constructed assets by key. reserve() provides storage whose slots
     * callers construct selectively with emplace(); the older allocate()
     * interface returns unconstructed handles for manual construction.
     * Publish complete assets under a unique lock; readers copy retained handles
     * under a shared lock. Construction and final release happen outside the lock.
     */
    template <typename AssetType, typename Key = std::filesystem::path>
    struct AssetMgr : AssetCacheContext {
        using spointer = std::shared_ptr<AssetType>;
        using key_type = Key;

    protected:
        mutable std::shared_mutex assets_mutex_;
        std::unordered_map<Key, spointer> loaded_assets{};

    public:
        AssetMgr() = default;
        virtual ~AssetMgr() { clear_assets(); }
        [[nodiscard]] virtual spointer get_asset(const Key& key) const {
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
        virtual void clear_assets() noexcept {
            decltype(loaded_assets) retired;
            {
                std::unique_lock lock(assets_mutex_);
                retired.swap(loaded_assets);
            }
            // A deleter can inspect this cache without re-entering its write lock.
        }

    protected:
        spointer publish_asset(const Key& key, spointer asset) {
            if (!asset)
                throw Exceptions::failed_operation(CE_HERE, "Cannot publish an empty asset");
            std::unique_lock lock(assets_mutex_);
            return loaded_assets.try_emplace(key, std::move(asset)).first->second;
        }
        spointer replace_asset(const Key& key, spointer asset) {
            if (!asset)
                throw Exceptions::failed_operation(CE_HERE, "Cannot publish an empty asset");
            spointer retired;
            spointer result;
            {
                std::unique_lock lock(assets_mutex_);
                auto entry = loaded_assets.try_emplace(key, spointer{}).first;
                retired = std::exchange(entry->second, std::move(asset));
                result = entry->second;
            }
            return result;
        }
        /** Reserve raw slots for selective construction with emplace(). */
        template <typename Derived>
        auto reserve(const std::size_t N) {
            static_assert(std::is_base_of_v<AssetType, Derived>);
            return Obj::ObjectReservation<Derived, Mem::ObjectPoolAllocator<Derived>>(N);
        }

        /** Provide raw object handles for callers that construct slots manually. */
        template <typename Derived>
        std::vector<std::shared_ptr<Derived>> allocate(const std::size_t N) {
            static_assert(std::is_base_of_v<AssetType, Derived>, "The allocated class type must be derived from the managed type.");
            if (N == 0) {
                return {};
            }
            using A_OPA = std::allocator_traits<Mem::ObjectPoolAllocator<Derived>>;
            Mem::ObjectPoolAllocator<Derived> allocator;
            auto context = allocator.context();
            // Allocate one raw batch, then hand out independent per-slot
            // handles; the pool context outlives this temporary allocator.
            auto raw = A_OPA::allocate(allocator, N);
            std::shared_ptr<Derived> owner(raw, [](void* p) {});
            Block<Derived> block{owner, owner, ptr::calculate_alignment(raw), N};
            // Manual callers must construct slots themselves. Their final
            // handle destroys its slot and returns exactly that slot to pool.
            return block.vector([context](Derived* p) noexcept {
                A_OPA::destroy(p);
                context->release_owned(p, 1);
            });
        }
    };
}
