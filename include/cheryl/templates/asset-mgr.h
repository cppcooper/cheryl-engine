#pragma once
#include <assets/abstracts.h>
#include <core/resources/allocators.h>
#include <core/resources/objects/object-reservation.hpp>
#include <internals/exceptions.h>

#include "block.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace CE::Assets {
    struct ResourceProvider;

    // Singleton asset managers use one resource provider for their process lifetime.
    // TODO: Give this binding an explicit cache/resource lifecycle. The process-global provider
    // pointer never resets, so backend replacement, multiple providers, and coordinated GPU
    // teardown cannot currently be represented by the cache model.
    // TODO: Include provider binding in the concurrency contract. bound_provider_ is unsynchronized, so
    // concurrent first loads or backend/cache lifecycle changes would race even before asset maps are touched.
    class ProviderBoundCache {
    public:
        static void verify_provider(const ResourceProvider& provider) {
            if (bound_provider_ && bound_provider_ != &provider)
                throw Exceptions::failed_operation(
                    CE_HERE, "Asset caches are already bound to another resource provider");
        }

    protected:
        static void bind_provider(const ResourceProvider& provider) {
            verify_provider(provider);
            bound_provider_ = &provider;
        }

    private:
        inline static const ResourceProvider* bound_provider_ = nullptr;
    };

    /**
     * Caches constructed assets by key. reserve() provides storage whose slots
     * callers construct selectively with emplace(); the older allocate()
     * interface returns unconstructed handles for manual construction.
     * TODO: Define synchronization/publication before background loading or hot reload. loaded_assets is an
     * ordinary unordered_map, so concurrent load/get/clear operations are data races even when the underlying
     * pooled object lifetime is otherwise safe.
     */
    template <typename AssetType, typename Key = std::filesystem::path>
    struct AssetMgr : ProviderBoundCache {
        using spointer = std::shared_ptr<AssetType>;
        using key_type = Key;
        AssetMgr() = default;
        virtual ~AssetMgr() { loaded_assets.clear(); }
        [[nodiscard]] virtual spointer get_asset(const Key& key) const {
            if (const auto asset = loaded_assets.find(key); asset != loaded_assets.end()) {
                return asset->second;
            }
            return nullptr;
        }
        [[nodiscard]] bool contains(const Key& key) const { return loaded_assets.contains(key); }
        [[nodiscard]] std::size_t size() const { return loaded_assets.size(); }

    protected:
        /** Reserve raw slots for selective construction with emplace(). */
        template <typename Derived>
        auto reserve(const std::size_t N) {
            static_assert(std::is_base_of_v<AssetType, Derived>);
            return Obj::ObjectReservation<Derived, Mem::ObjectPoolAllocator<Derived>>(N);
        }

        /** Provide raw object handles for callers that construct slots manually. */
        template <typename Derived>
        std::vector<std::shared_ptr<Derived>> allocate(const std::size_t N) {
            static_assert(std::is_base_of_v<AssetType, Derived>,
                          "The allocated class type must be derived from the managed type.");
            if (N == 0) {
                return {};
            }
            using A_OPA = std::allocator_traits<Mem::ObjectPoolAllocator<Derived>>;
            Mem::ObjectPoolAllocator<Derived> allocator;
            auto context = allocator.context();
            auto raw = A_OPA::allocate(allocator, N);
            std::shared_ptr<Derived> owner(raw, [](void* p) {});
            Block<Derived> block{owner, owner, ptr::calculate_alignment(raw), N};
            return block.vector([context](Derived* p) noexcept {
                A_OPA::destroy(p);
                context->release_owned(p, 1);
            });
        }
        std::unordered_map<Key, spointer> loaded_assets{};
    };
}
