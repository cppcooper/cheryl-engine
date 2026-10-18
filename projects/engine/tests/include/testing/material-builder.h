#pragma once

#include <assets/resources/shader-asset-builder.h>

#include <cstddef>
#include <map>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace CE::Testing {
    // Records provider requests; tests supply resource handles independently.
    struct MaterialBuilder final : CE::Assets::Shaders::Builder {
        std::map<CE::Rendering::Materials::Builtin2D, std::shared_ptr<const CE::Rendering::Resources::Material>> values;
        std::vector<std::pair<CE::Rendering::Materials::Builtin2D, CE::Rendering::Pipelines::Blend>> requests;
        std::vector<std::thread::id> owners;
        std::size_t fail_request = 0;
        bool missing = false;

        void validate_program(const CE::Assets::Shaders::ProgramRecipe&) const override {
            throw std::logic_error("Built-in requests must not require application shader recipes");
        }
        void validate_material(const CE::Assets::Shaders::MaterialRecipe&, const CE::Assets::Shaders::ProgramRecipe&) const override {
            throw std::logic_error("Built-in requests must not require application material recipes");
        }
        std::shared_ptr<CE::Rendering::Resources::Program> build_program(const CE::Assets::Shaders::ProgramRecipe&) override {
            throw std::logic_error("Built-in requests must not require application shader compilation");
        }
        std::shared_ptr<const CE::Rendering::Resources::Material> build_material(
            const CE::Assets::Shaders::MaterialRecipe&,
            const CE::Assets::Shaders::ProgramRecipe&,
            const std::shared_ptr<CE::Rendering::Resources::Program>&
        ) override {
            throw std::logic_error("Built-in requests must not require application material construction");
        }
        std::shared_ptr<const CE::Rendering::Resources::Material>
        build_builtin_material(const CE::Rendering::Materials::Builtin2D kind, const CE::Rendering::Pipelines::Blend blend) override {
            requests.emplace_back(kind, blend);
            owners.push_back(std::this_thread::get_id());
            if (requests.size() == fail_request)
                throw std::runtime_error("Injected built-in material failure");
            return missing ? nullptr : values.at(kind);
        }
    };
}
