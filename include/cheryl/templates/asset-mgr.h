#pragma once
#ifndef ASSETMGR_TEMPLATE_H
#define ASSETMGR_TEMPLATE_H
#include <assets/abstracts.h>
#include <resources/allocators.h>
#include <unordered_map>
#include <filesystem>
#include <vector>
#include <memory>

namespace CE::Assets {
    struct iAssetMgr {
        virtual ~iAssetMgr() = default;
        virtual void load_assets(const std::vector<fs::path>& file) = 0;
    };

    template<typename AssetType>
    struct AssetMgr : iAssetMgr {
        using spointer = std::shared_ptr<AssetType>;
        AssetMgr() = default;
        ~AssetMgr() override {
            loaded_assets.clear();
        }
        virtual spointer get_asset(const fs::path& f) {
            if (loaded_assets.count(f)) {
                return loaded_assets[f];
            }
            return nullptr;
        };

    protected:
        template<typename Derived>
        std::vector<std::shared_ptr<Derived>> allocate(const std::size_t N) {
            static_assert(std::is_base_of_v<AssetType, Derived>, "The allocated class type must be derived from the managed type.");
            using A_OPA = std::allocator_traits<Mem::ObjectPoolAllocator<Derived>>;
            auto raw = A_OPA::allocate(N);
            std::shared_ptr<Derived> owner(raw,[](void* p) {});
            Block<Derived> block{owner,owner,ptr::calculate_alignment(raw),N};
            return block.vector([](Derived* p) {
                A_OPA::destroy(p);
                A_OPA::deallocate(p,1);
            });
        }
        std::unordered_map<fs::path, spointer> loaded_assets{};
    };
}
#endif
