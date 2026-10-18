#pragma once

#include <cheryl/export.h>

#include "rendering.h"

#include <assets/resources/resource-provider.h>
#include <core/engine/platform-dispatcher.h>
#include <core/rendering/render-frame.h>

#include <cstdint>
#include <future>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace CE::UI::RmlUi {
    // Colored triangles, premultiplied alpha, disabled depth/write/culling and
    // Cheryl's projection semantic. Textured shaders multiply image/vertex RGBA.
    struct Materials {
        std::shared_ptr<const CE::Rendering::Resources::Material> solid;
        std::shared_ptr<const CE::Rendering::Resources::Material> textured;
        std::string image_parameter = "image";
        std::uint32_t image_unit = 0;
        CE::Rendering::Parameters::Set pass_parameters;
    };

    class Scene final {
        friend class SceneUploader;
        glm::mat4 projection_{1};
        CE::Rendering::Parameters::Set pass_parameters_;
        std::vector<CE::Rendering::DrawPacket2D> draws_;

    public:
        [[nodiscard]] std::span<const CE::Rendering::DrawPacket2D> draws() const { return draws_; }
        CE_UI_RMLUI_API void write(CE::Rendering::FrameWriter& frame) const;
    };

    // Construct on the provider's platform owner. Copies may submit owned CPU
    // recordings from UI; upload/cache access stays on that provider and owner.
    class SceneUploader final {
        struct State;
        std::shared_ptr<State> state_;

    public:
        CE_UI_RMLUI_API explicit SceneUploader(const CE::Rendering::Resources::Provider& provider);
        // Defaults are built lazily on the provider owner from backend-owned
        // resources. Explicit Materials select application overrides instead.
        [[nodiscard]] CE_UI_RMLUI_API Scene upload(CE::Rendering::Resources::Provider& provider, const RecordedScene& recording) const;
        [[nodiscard]] CE_UI_RMLUI_API Scene
        upload(CE::Rendering::Resources::Provider& provider, const RecordedScene& recording, const Materials& materials) const;
        [[nodiscard]] CE_UI_RMLUI_API std::future<Scene>
        submit(const CE::Runtime::PlatformDispatcher::Submission& platform, RecordedScene recording) const;
        [[nodiscard]] CE_UI_RMLUI_API std::future<Scene>
        submit(const CE::Runtime::PlatformDispatcher::Submission& platform, RecordedScene recording, Materials materials) const;
    };

    // Nonblocking adoption of one complete replacement. Failure preserves current.
    CE_UI_RMLUI_API bool adopt_scene(std::future<Scene>& completion, Scene& current);
}
