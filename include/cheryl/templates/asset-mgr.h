#pragma once
#include <assets/abstracts.h>
#include <core/resources/allocators.h>
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
        template <typename Derived>
        std::vector<std::shared_ptr<Derived>> allocate(const std::size_t N) {
            static_assert(std::is_base_of_v<AssetType, Derived>,
                          "The allocated class type must be derived from the managed type.");
            if (N == 0) {
                return {};
            }
            using A_OPA = std::allocator_traits<Mem::ObjectPoolAllocator<Derived>>;
            auto raw = A_OPA::allocate(N);
            std::shared_ptr<Derived> owner(raw, [](void* p) {});
            Block<Derived> block{owner, owner, ptr::calculate_alignment(raw), N};
            return block.vector([](Derived* p) {
                A_OPA::destroy(p);
                A_OPA::deallocate(p, 1);
            });
        }
        std::unordered_map<Key, spointer> loaded_assets{};
    };
}
