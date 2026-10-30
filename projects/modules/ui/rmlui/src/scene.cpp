#include <ui/rmlui/scene.h>

#include <assets/resources/shader-asset-builder.h>

#include <core/engine/engine-context.h>
#include <ext/matrix_clip_space.hpp>
#include <internals/exceptions.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <thread>
#include <utility>

namespace CE::UI::RmlUi {
    namespace {
        CE::Rendering::Pipelines::PassConstraints2D ui_constraints() {
            return {
                CE::Rendering::Pipelines::Blend::PremultipliedAlpha, CE::Rendering::Pipelines::Depth::Disabled, false,
                CE::Rendering::Pipelines::Cull::None
            };
        }

        void validate_material(const std::shared_ptr<const CE::Rendering::Resources::Material>& material) {
            if (!material)
                throw Exceptions::invalid_args(CE_HERE, "RmlUi draw needs a compatible material");
            const auto& pipeline = material->definition().pipeline->definition();
            const auto& state = pipeline.state;
            if (pipeline.vertex_layout != CE::Rendering::Geometry::Layout2D::Position3UV2Color4 ||
                pipeline.topology != CE::Rendering::Geometry::Topology::Triangles ||
                state.blend != CE::Rendering::Pipelines::Blend::PremultipliedAlpha ||
                state.depth != CE::Rendering::Pipelines::Depth::Disabled || state.depth_write ||
                state.cull != CE::Rendering::Pipelines::Cull::None)
                throw Exceptions::invalid_args(
                    CE_HERE, "RmlUi material needs colored triangles with premultiplied alpha and disabled depth/culling"
                );
            if (std::none_of(
                    pipeline.parameters.begin(), pipeline.parameters.end(), [](const CE::Rendering::Parameters::Definition& parameter) {
                        return parameter.semantic == CE::Rendering::Parameters::Semantic::Projection;
                    }
                ))
                throw Exceptions::invalid_args(CE_HERE, "RmlUi material needs Cheryl's projection semantic for its logical coordinates");
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
            const auto parameter =
                std::find_if(parameters.begin(), parameters.end(), [&](const CE::Rendering::Parameters::Definition& value) {
                    return value.key == materials.image_parameter;
                });
            if (parameter == parameters.end() || parameter->type != CE::Rendering::Parameters::Type::Sampler2D ||
                parameter->semantic != CE::Rendering::Parameters::Semantic::Custom)
                throw Exceptions::invalid_args(CE_HERE, "RmlUi image parameter must name a custom Sampler2D in the textured material");
        }
    }

    struct SceneUploader::State {
        const CE::Diagnostics::DomainId domain;
        const std::thread::id owner;
        Materials defaults;
        std::map<std::weak_ptr<const CE::Assets::Images::Decoded>, std::weak_ptr<const CE::Rendering::Resources::Image>, std::owner_less<>>
            images;

        explicit State(const CE::Rendering::Resources::Provider& provider)
        : domain(provider.diagnostic_id()), owner(std::this_thread::get_id()) {}
    };

    SceneUploader::SceneUploader(const CE::Rendering::Resources::Provider& provider)
    : state_(std::make_shared<State>(provider)) {}

    Scene SceneUploader::upload(CE::Rendering::Resources::Provider& provider, const RecordedScene& recording) const {
        if (std::this_thread::get_id() != state_->owner || provider.diagnostic_id() != state_->domain)
            throw Exceptions::failed_operation(CE_HERE, "RmlUi upload requires its original provider and platform owner");
        if (recording.draws().empty())
            return Scene{};
        if (!state_->defaults.solid || !state_->defaults.textured) {
            auto* builder = provider.shader_asset_builder();
            if (!builder)
                throw Exceptions::failed_operation(CE_HERE, "RmlUi defaults require a provider with built-in 2D materials");
            Materials candidate;
            candidate.solid = builder->build_builtin_material(
                CE::Rendering::Materials::Builtin2D::SolidColor, CE::Rendering::Pipelines::Blend::PremultipliedAlpha
            );
            candidate.textured = builder->build_builtin_material(
                CE::Rendering::Materials::Builtin2D::TexturedColor, CE::Rendering::Pipelines::Blend::PremultipliedAlpha
            );
            if (!candidate.solid || !candidate.textured)
                throw Exceptions::failed_operation(CE_HERE, "RmlUi provider returned missing built-in 2D materials");
            validate_materials(recording, candidate);
            state_->defaults = std::move(candidate);
        }
        return upload(provider, recording, state_->defaults);
    }

    Scene
    SceneUploader::upload(CE::Rendering::Resources::Provider& provider, const RecordedScene& recording, const Materials& materials) const {
        if (std::this_thread::get_id() != state_->owner || provider.diagnostic_id() != state_->domain)
            throw Exceptions::failed_operation(CE_HERE, "RmlUi upload requires its original provider and platform owner");
        validate_materials(recording, materials);
        Scene scene;
        if (recording.draws().empty())
            return scene;
        if (!std::isfinite(2.0f / recording.width()) || !std::isfinite(2.0f / recording.height()))
            throw Exceptions::invalid_args(CE_HERE, "RmlUi logical extent cannot be represented by a projection");
        scene.projection_ = glm::ortho(0.0f, recording.width(), recording.height(), 0.0f);
        scene.pass_parameters_ = materials.pass_parameters;
        scene.draws_.reserve(recording.draws().size());
        const CE::Rendering::Parameters::PassSemantics pass{scene.projection_, glm::mat4{1}};
        const auto constraints = ui_constraints();
        std::erase_if(state_->images, [](const auto& entry) { return entry.first.expired() || entry.second.expired(); });
        for (const auto& draw : recording.draws()) {
            CE::Rendering::DrawStyle2D style;
            style.material = draw.texture ? materials.textured : materials.solid;
            style.clip = draw.clip;
            if (draw.texture) {
                const std::weak_ptr<const CE::Assets::Images::Decoded> generation = draw.texture;
                auto& cached = state_->images[generation];
                auto image = cached.lock();
                if (!image) {
                    image = provider.create_image(*draw.texture);
                    if (!image || image->pixel_size().width != draw.texture->size.width ||
                        image->pixel_size().height != draw.texture->size.height)
                        throw Exceptions::failed_operation(CE_HERE, "RmlUi image upload returned missing or incompatible metadata");
                    cached = image;
                }
                style.parameters.emplace(
                    materials.image_parameter, CE::Rendering::Parameters::ImageBinding{std::move(image), materials.image_unit}
                );
            }
            auto geometry = provider.upload_geometry(draw.vertices, CE::Rendering::Geometry::Topology::Triangles);
            auto packet = CE::Rendering::resolve_draw_packet(
                std::move(geometry), 0, draw.vertices.size(), style, pass, scene.pass_parameters_, constraints
            );
            packet.authored_order = scene.draws_.size();
            scene.draws_.push_back(std::move(packet));
        }
        return scene;
    }

    std::future<Scene> SceneUploader::submit(const CE::Runtime::PlatformDispatcher::Submission& platform, RecordedScene recording) const {
        return platform.submit([uploader = *this, recording = std::move(recording)](CE::Runtime::Context& engine) {
            return uploader.upload(engine.resources(), recording);
        });
    }

    std::future<Scene>
    SceneUploader::submit(const CE::Runtime::PlatformDispatcher::Submission& platform, RecordedScene recording, Materials materials) const {
        return platform.submit([uploader = *this, recording = std::move(recording),
                                materials = std::move(materials)](CE::Runtime::Context& engine) {
            return uploader.upload(engine.resources(), recording, materials);
        });
    }

    void Scene::write(CE::Rendering::FrameWriter& frame) const {
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
