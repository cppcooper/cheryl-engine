#pragma once

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
    // Shader code modulates image RGBA by vertex RGBA. Other custom values come
    // from material defaults or this copied pass layer.
    struct Materials {
        std::shared_ptr<const Assets::Material> solid;
        std::shared_ptr<const Assets::Material> textured;
        std::string image_parameter = "image";
        std::uint32_t image_unit = 0;
        Assets::ParameterSet pass_parameters;
    };

    class Scene final {
        friend class SceneUploader;
        glm::mat4 projection_{1};
        Assets::ParameterSet pass_parameters_;
        std::vector<RenderAPIs::DrawPacket2D> draws_;

    public:
        [[nodiscard]] std::span<const RenderAPIs::DrawPacket2D> draws() const { return draws_; }
        // Append one UI pass at the caller's chosen position in frame order.
        void write(RenderAPIs::RenderFrameWriter& frame) const;
    };

    // Construct on the provider's platform owner, then copy the handle to UI.
    // submit() captures owned CPU data and materials, never live toolkit objects.
    // Upload/cache access is restricted to that owner and provider domain.
    class SceneUploader final {
        struct State;
        std::shared_ptr<State> state_;

    public:
        explicit SceneUploader(const Assets::ResourceProvider& provider);
        [[nodiscard]] Scene upload(Assets::ResourceProvider& provider, const RecordedScene& recording, const Materials& materials) const;
        [[nodiscard]] std::future<Scene>
        submit(const Engine::PlatformDispatcher::Submission& platform, RecordedScene recording, Materials materials) const;
    };

    // Call during update. Returns false without waiting while work is pending.
    // A ready error (including dispatcher cancellation) propagates and leaves
    // current unchanged. Success replaces it with one complete retained scene.
    bool adopt_scene(std::future<Scene>& completion, Scene& current);
}
