#include <ui/tgui/scene.h>

#include "dependency-contract.h"

#include <core/engine/engine-context.h>
#include <ext/matrix_clip_space.hpp>
#include <internals/exceptions.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <thread>
#include <utility>

namespace CE::UI::TGUI {
    namespace {
        Assets::PassConstraints2D ui_constraints() {
            return {Assets::BlendMode::StraightAlpha, Assets::DepthMode::Disabled, false, Assets::CullMode::None};
        }

        void validate_material(const std::shared_ptr<const Assets::Material>& material) {
            if (!material)
                throw Exceptions::invalid_args(CE_HERE, "TGUI draw needs an application-supplied material");
            const auto& pipeline = material->definition().pipeline->definition();
            const auto& state = pipeline.state;
            if (pipeline.vertex_layout != Assets::VertexLayout2D::Position3UV2Color4 ||
                pipeline.topology != Assets::PrimitiveTopology::Triangles || state.blend != Assets::BlendMode::StraightAlpha ||
                state.depth != Assets::DepthMode::Disabled || state.depth_write || state.cull != Assets::CullMode::None)
                throw Exceptions::invalid_args(
                    CE_HERE, "TGUI material needs colored triangles with straight alpha and disabled depth/culling"
                );
            if (std::none_of(pipeline.parameters.begin(), pipeline.parameters.end(), [](const Assets::ParameterDefinition& parameter) {
                    return parameter.semantic == Assets::ParameterSemantic::Projection;
                }))
                throw Exceptions::invalid_args(CE_HERE, "TGUI material needs Cheryl's projection semantic for its logical coordinates");
        }

        void validate_materials(const RecordedScene& recording, const Materials& materials) {
            bool solid = false;
            bool textured = false;
            for (const auto& draw : recording.draws()) {
                solid = solid || !draw.texture;
                textured = textured || static_cast<bool>(draw.texture);
            }
            if (solid)
                validate_material(materials.solid);
            if (!textured)
                return;
            validate_material(materials.textured);
            const auto& parameters = materials.textured->definition().pipeline->definition().parameters;
            const auto parameter = std::find_if(parameters.begin(), parameters.end(), [&](const Assets::ParameterDefinition& value) {
                return value.key == materials.image_parameter;
            });
            if (parameter == parameters.end() || parameter->type != Assets::ParameterType::Sampler2D ||
                parameter->semantic != Assets::ParameterSemantic::Custom)
                throw Exceptions::invalid_args(CE_HERE, "TGUI image parameter must name a custom Sampler2D in the textured material");
        }
    }

    struct SceneUploader::State {
        const Diagnostics::DomainId domain;
        const std::thread::id owner;
        std::map<std::weak_ptr<const Assets::DecodedImage>, std::weak_ptr<const Assets::Image>, std::owner_less<>> images;

        explicit State(const Assets::ResourceProvider& provider)
        : domain(provider.diagnostic_id()), owner(std::this_thread::get_id()) {}
    };

    SceneUploader::SceneUploader(const Assets::ResourceProvider& provider)
    : state_(std::make_shared<State>(provider)) {}

    Scene SceneUploader::upload(Assets::ResourceProvider& provider, const RecordedScene& recording, const Materials& materials) const {
        if (std::this_thread::get_id() != state_->owner || provider.diagnostic_id() != state_->domain)
            throw Exceptions::failed_operation(CE_HERE, "TGUI upload requires its original provider and platform owner");
        validate_materials(recording, materials);
        Scene scene;
        if (recording.draws().empty())
            return scene;
        if (!std::isfinite(2.0f / recording.width()) || !std::isfinite(2.0f / recording.height()))
            throw Exceptions::invalid_args(CE_HERE, "TGUI logical extent cannot be represented by a projection");
        scene.projection_ = glm::ortho(0.0f, recording.width(), recording.height(), 0.0f);
        scene.pass_parameters_ = materials.pass_parameters;
        scene.draws_.reserve(recording.draws().size());
        const Assets::ShaderPass pass{scene.projection_, glm::mat4{1}};
        const auto constraints = ui_constraints();
        std::erase_if(state_->images, [](const auto& entry) { return entry.first.expired() || entry.second.expired(); });
        for (const auto& draw : recording.draws()) {
            RenderAPIs::DrawStyle2D style;
            style.material = draw.texture ? materials.textured : materials.solid;
            style.clip = draw.clip;
            if (draw.texture) {
                const std::weak_ptr<const Assets::DecodedImage> generation = draw.texture;
                auto& cached = state_->images[generation];
                auto image = cached.lock();
                if (!image) {
                    image = provider.create_image(*draw.texture);
                    if (!image || image->pixel_size().width != draw.texture->size.width ||
                        image->pixel_size().height != draw.texture->size.height)
                        throw Exceptions::failed_operation(CE_HERE, "TGUI image upload returned missing or incompatible metadata");
                    cached = image;
                }
                style.parameters.emplace(materials.image_parameter, Assets::ImageBinding{std::move(image), materials.image_unit});
            }
            auto geometry = provider.upload_geometry(draw.vertices, Assets::PrimitiveTopology::Triangles);
            auto packet = RenderAPIs::resolve_draw_packet(
                std::move(geometry), 0, draw.vertices.size(), style, pass, scene.pass_parameters_, constraints
            );
            packet.authored_order = scene.draws_.size();
            scene.draws_.push_back(std::move(packet));
        }
        return scene;
    }

    std::future<Scene>
    SceneUploader::submit(const Engine::PlatformDispatcher::Submission& platform, RecordedScene recording, Materials materials) const {
        return platform.submit([uploader = *this, recording = std::move(recording),
                                materials = std::move(materials)](Engine::EngineContext& engine) {
            return uploader.upload(engine.resources(), recording, materials);
        });
    }

    void Scene::write(RenderAPIs::RenderFrameWriter& frame) const {
        if (draws_.empty())
            return;
        auto pass = frame.begin_pass(projection_, glm::mat4{1}, ui_constraints(), pass_parameters_);
        pass.add(draws_);
    }

    bool adopt_scene(std::future<Scene>& completion, Scene& current) {
        if (!completion.valid() || completion.wait_for(std::chrono::seconds{0}) != std::future_status::ready)
            return false;
        auto replacement = completion.get();
        current = std::move(replacement);
        return true;
    }
}
