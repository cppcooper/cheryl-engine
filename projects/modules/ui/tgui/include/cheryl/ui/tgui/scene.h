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

namespace CE::UI::TGUI {
    // Application-built pipelines must accept colored triangles, straight alpha,
    // disabled depth/write/culling, Cheryl's projection semantic, and the named
    // custom Sampler2D parameter for textured draws.
    // CE::Rendering::Resources::Program code modulates image RGBA by vertex RGBA. Other custom values come
    // from material defaults or this copied pass layer.
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
        // Append one UI pass at the caller's chosen position in frame order.
        CE_UI_TGUI_API void write(CE::Rendering::FrameWriter& frame) const;
    };

    // Construct on the provider's platform owner, then copy the handle to UI.
    // submit() captures owned CPU data and materials, never live toolkit objects.
    // Upload/cache access is restricted to that owner and provider domain.
    class SceneUploader final {
        struct State;
        std::shared_ptr<State> state_;

    public:
        CE_UI_TGUI_API explicit SceneUploader(const CE::Rendering::Resources::Provider& provider);
        [[nodiscard]] CE_UI_TGUI_API Scene
        upload(CE::Rendering::Resources::Provider& provider, const RecordedScene& recording, const Materials& materials) const;
        [[nodiscard]] CE_UI_TGUI_API std::future<Scene>
        submit(const CE::Runtime::PlatformDispatcher::Submission& platform, RecordedScene recording, Materials materials) const;
    };

    // Call during update. Returns false without waiting while work is pending.
    // A ready error (including dispatcher cancellation) propagates and leaves
    // current unchanged. Success replaces it with one complete retained scene.
    CE_UI_TGUI_API bool adopt_scene(std::future<Scene>& completion, Scene& current);
}
