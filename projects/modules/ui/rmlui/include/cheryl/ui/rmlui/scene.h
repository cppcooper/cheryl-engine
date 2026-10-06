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

namespace CE::UI::RmlUi {
    // Colored triangles, premultiplied alpha, disabled depth/write/culling and
    // Cheryl's projection semantic. Textured shaders multiply image/vertex RGBA.
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
        void write(RenderAPIs::RenderFrameWriter& frame) const;
    };

    // Construct on the provider's platform owner. Copies may submit owned CPU
    // recordings from UI; upload/cache access stays on that provider and owner.
    class SceneUploader final {
        struct State;
        std::shared_ptr<State> state_;

    public:
        explicit SceneUploader(const Assets::ResourceProvider& provider);
        [[nodiscard]] Scene upload(Assets::ResourceProvider& provider, const RecordedScene& recording, const Materials& materials) const;
        [[nodiscard]] std::future<Scene>
        submit(const Engine::PlatformDispatcher::Submission& platform, RecordedScene recording, Materials materials) const;
    };

    // Nonblocking adoption of one complete replacement. Failure preserves current.
    bool adopt_scene(std::future<Scene>& completion, Scene& current);
}
