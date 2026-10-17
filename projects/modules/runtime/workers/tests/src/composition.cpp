#include "shared-pool.h"
#include <core/controls/input-interface.h>
#include <core/display/display-system-interface.h>
#include <core/rendering/presentation-surface.h>
#include <core/rendering/renderer.h>
#include <assets/resources/resource-provider.h>
#include <internals/exceptions.h>
#include <testing/worker-topology.h>
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <utility>

namespace {
    class Display final : public CE::Platform::DisplaySystem {
        std::vector<CE::Platform::Monitor> monitors_{{1, 16, 16}};

    public:
        const std::vector<CE::Platform::Monitor>& monitors() const override { return monitors_; }
        int monitor_count() const override { return 1; }
        const CE::Platform::Monitor& primary_monitor() const override { return monitors_.front(); }
        CE::Platform::Window* active_window() const override { return nullptr; }
        std::pair<float, float> content_scale(const CE::Platform::Monitor&) const override { return {1, 1}; }
        CE::Platform::Window* create_window(const CE::Platform::Monitor&, CE::Platform::WindowMode, int, int) override { return nullptr; }
        void activate_window(CE::Platform::Window&) override {}
    };
    class Surface final : public CE::Rendering::PresentationSurface {
    public:
        void present() override {}
    };
    class Renderer final : public CE::Rendering::Renderer {
    public:
        void initialize() override {}
        void deinitialize() override {}
        void maintain_resources() override {}
        void render(const CE::Rendering::Frame&) override {}
        void clear() override {}
        void set_viewport(CE::Platform::FramebufferSize) override {}
        void set_depth_test(bool) override {}
        void set_clear_colour(float, float, float, float) override {}
        void set_camera_matrices(const glm::mat4&, const glm::mat4&) override {}
    };
    class Resources final : public CE::Rendering::Resources::Provider {
    public:
        std::shared_ptr<CE::Rendering::Resources::Image> create_image(const CE::Assets::Images::Decoded&) override {
            throw std::logic_error("Unused");
        }
        std::shared_ptr<CE::Rendering::Resources::Image>
        create_font_atlas(std::span<const unsigned char>, CE::Rendering::Geometry::PixelSize) override {
            throw std::logic_error("Unused");
        }
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D>
        upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2D>, CE::Rendering::Geometry::Topology) override {
            throw std::logic_error("Unused");
        }
        std::shared_ptr<CE::Rendering::Resources::Program> link_program(const std::vector<std::filesystem::path>&) override {
            throw std::logic_error("Unused");
        }
    };
    class Input final : public CE::Input::System {
        CE::Input::Actions::Bindings bindings_;

    public:
        void initialize(CE::Platform::Window&) override {}
        void deinitialize() override {}
        void poll() override {}
        CE::Input::Actions::Bindings& bindings() override { return bindings_; }
        CE::Input::DeviceId keyboard_id() const override { return 1; }
        CE::Input::DeviceId mouse_id() const override { return 2; }
        CE::Input::DeviceId gamepad_id() const override { return 3; }
    };

    std::unique_ptr<CE::Runtime::Context> context(CE::Runtime::ExecutionOptions execution) {
        return std::make_unique<CE::Runtime::Context>(
            std::make_unique<Display>(), std::make_unique<Surface>(), std::make_unique<Renderer>(), std::make_unique<Resources>(),
            std::make_unique<Input>(), std::move(execution)
        );
    }
}

TEST(worker_composition, shared_root) {
    CE::Workers::Configuration configuration;
    auto provider = std::make_shared<CE::Testing::WorkerTopologyProvider>();
    configuration.pool = {2, provider};
    configuration.groups["simulation"].max_concurrency = 1;
    configuration.groups["simulation"].cpu.locality = CE::Workers::LocalityPolicy::Reuse;
    configuration.groups["asset_preparation"].cpu.locality = CE::Workers::LocalityPolicy::Spread;
    auto composition = CE::Examples::compose_workers(configuration, context);
    auto independent = composition.pool->make_group();
    auto simulation = composition.groups.at("simulation");
    auto assets = composition.groups.at("asset_preparation");
    EXPECT_EQ(simulation.status().pool, composition.pool->diagnostic_id());
    EXPECT_EQ(assets.status().pool, composition.pool->diagnostic_id());
    EXPECT_EQ(simulation.policy().effective_locality, CE::Workers::LocalityPolicy::Reuse);
    EXPECT_EQ(assets.policy().effective_locality, CE::Workers::LocalityPolicy::Spread);
    auto capture = std::make_shared<int>(17);
    const std::weak_ptr<int> lifetime = capture;
    auto result = simulation.submit([capture = std::move(capture)] { return *capture; });
    composition.context.reset();
    EXPECT_EQ(result.get(), 17);
    EXPECT_TRUE(lifetime.expired());
    EXPECT_FALSE(simulation.status().accepting);
    EXPECT_FALSE(assets.status().accepting);
    EXPECT_TRUE(independent.status().accepting);
    EXPECT_EQ(independent.submit([] { return 19; }).get(), 19);
    EXPECT_THROW(static_cast<void>(simulation.submit([] {})), CE::Exceptions::failed_operation);
}

TEST(worker_composition, rejected_startup) {
    CE::Workers::Configuration configuration;
    auto provider = std::make_shared<CE::Testing::WorkerTopologyProvider>();
    configuration.pool.topology_provider = provider;
    configuration.groups["Bad"] = {};
    EXPECT_THROW(static_cast<void>(CE::Examples::compose_workers(configuration, context)), CE::Exceptions::invalid_args);
    EXPECT_EQ(provider->discoveries, 0u);
    configuration.groups.clear();
    EXPECT_THROW(static_cast<void>(CE::Examples::compose_workers(configuration, {})), CE::Exceptions::invalid_args);
    EXPECT_EQ(provider->discoveries, 0u);
    EXPECT_THROW(
        static_cast<void>(CE::Examples::compose_workers(configuration, [](auto) { return nullptr; })), CE::Exceptions::failed_operation
    );
    EXPECT_EQ(provider.use_count(), 2); // Only this test and its configuration remain.
}
