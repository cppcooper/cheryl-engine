#include <assets/resources/resource-provider.h>
#include <core/resources/asset-management/asset-loader.h>
#include <core/resources/asset-management/file-registry.h>
#include <internals/exceptions.h>
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
    namespace Loading = CE::Assets::Loading;
    using Outcome = Loading::UploadOutcome;
    using Event = Loading::AssetUploadSettled;

    struct MemoryImage final : CE::Rendering::Resources::Image {
        CE::Rendering::Geometry::PixelSize pixel_size() const override { return {1, 1}; }
        void bind(std::uint32_t) const override {}
    };

    struct Provider final : CE::Rendering::Resources::Provider {
        int uploads = 0;
        int fail_at = 0;
        std::shared_ptr<CE::Rendering::Resources::Image> create_image(const CE::Assets::Images::Decoded&) override {
            if (++uploads == fail_at)
                throw std::runtime_error("upload rejected");
            return std::make_shared<MemoryImage>();
        }
        std::shared_ptr<CE::Rendering::Resources::Image>
        create_font_atlas(std::span<const unsigned char>, CE::Rendering::Geometry::PixelSize) override {
            throw std::logic_error("No font upload in this fixture");
        }
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D>
        upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2D>, CE::Rendering::Geometry::Topology) override {
            throw std::logic_error("No geometry upload in this fixture");
        }
        std::shared_ptr<CE::Rendering::Resources::Program> link_program(const std::vector<std::filesystem::path>&) override {
            throw std::logic_error("No program upload in this fixture");
        }
    };

    Loading::Prepared images(std::size_t count, std::uint64_t source) {
        Loading::Prepared result;
        for (std::size_t index = 0; index < count; ++index)
            result.images.push_back(
                {"upload-events-" + std::to_string(source) + '-' + std::to_string(index), {{1, 1}, {255, 255, 255, 255}}}
            );
        return result;
    }
}

TEST(asset_upload_event, aggregate) {
    CE::Events::EventBus bus;
    Provider provider;
    Loading::Loader loader("unused-upload-root");
    loader.publish_events_to(bus.publisher());
    std::vector<Event> events;
    const auto id = bus.register_listener(Loading::asset_upload_settled_event, [&](const Event& event) {
        EXPECT_TRUE(loader.diagnostics().completed); // Callback is outside the observation lock and after completion.
        events.push_back(event);
    });
    const auto batch = images(10000, loader.source_id());
    loader.upload(batch, provider);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().source, loader.source_id());
    EXPECT_EQ(events.front().counts.batch, batch.batch);
    EXPECT_EQ(events.front().counts.images_completed, 10000u);
    EXPECT_EQ(events.front().counts.publications, 10000u);
    EXPECT_EQ(events.front().outcome, Outcome::Succeeded);
    EXPECT_FALSE(events.front().failure);
    EXPECT_EQ(provider.uploads, 10000);
    loader.upload(batch, provider);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_NE(events.front().attempt, events.back().attempt);
    EXPECT_EQ(events.front().counts.batch, events.back().counts.batch);
    EXPECT_EQ(events.back().counts.publications, 0u);
    EXPECT_EQ(events.back().outcome, Outcome::Succeeded);
    EXPECT_EQ(provider.uploads, 10000);
    EXPECT_TRUE(bus.unregister_and_wait(id));
}

TEST(asset_upload_event, failed_before_effects) {
    CE::Events::EventBus bus;
    Provider active;
    Provider wrong;
    Loading::Loader loader("unused-upload-root");
    loader.publish_events_to(bus.publisher());
    std::vector<Event> events;
    const auto id = bus.register_listener(Loading::asset_upload_settled_event, [&](const Event& event) { events.push_back(event); });
    loader.upload(images(1, loader.source_id()), active);
    ASSERT_EQ(active.uploads, 1); // Establish the provider-bound cache before rejecting another provider.
    EXPECT_THROW(loader.upload({}, wrong), CE::Exceptions::failed_operation);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events.back().outcome, Outcome::FailedBeforeEffects);
    EXPECT_FALSE(events.back().counts.completed);
    EXPECT_EQ(events.back().counts.publications, 0u);
    ASSERT_TRUE(events.back().failure);
    EXPECT_THROW(std::rethrow_exception(events.back().failure), CE::Exceptions::failed_operation);
    EXPECT_TRUE(bus.unregister_and_wait(id));
}

TEST(asset_upload_event, partial_and_isolated) {
    CE::Events::EventBus bus;
    Provider provider;
    provider.fail_at = 2;
    Loading::Loader loader("unused-upload-root");
    loader.publish_events_to(bus.publisher());
    bus.register_listener(Loading::asset_upload_settled_event, [](const Event&) { throw std::logic_error("observer"); });
    std::vector<Event> events;
    const auto id = bus.register_listener(Loading::asset_upload_settled_event, [&](const Event& event) { events.push_back(event); });
    auto batch = images(2, loader.source_id());
    try {
        loader.upload(batch, provider);
        FAIL() << "The second provider upload should fail";
    } catch (const std::runtime_error& failure) {
        EXPECT_STREQ(failure.what(), "upload rejected");
    }
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().outcome, Outcome::Partial);
    EXPECT_EQ(events.front().counts.images_completed, 1u);
    EXPECT_EQ(events.front().counts.publications, 1u);
    EXPECT_TRUE(events.front().counts.publication_count_available);
    EXPECT_THROW(std::rethrow_exception(events.front().failure), std::runtime_error);
    EXPECT_NO_THROW(loader.upload(batch, provider));
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events.back().outcome, Outcome::Succeeded);
    EXPECT_TRUE(bus.unregister_and_wait(id));
}

TEST(asset_upload_event, registry_with_zero_counts) {
    CE::Events::EventBus bus;
    Provider provider;
    provider.fail_at = 1;
    Loading::Loader loader("unused-upload-root");
    loader.publish_events_to(bus.publisher());
    auto registry = std::make_shared<Loading::FileRegistry>();
    const auto path = std::filesystem::absolute("upload-registry-" + std::to_string(loader.source_id()) + ".dat");
    registry->register_files(std::vector<std::filesystem::path>{path});
    auto batch = images(1, loader.source_id());
    batch.files = registry;
    std::vector<Event> events;
    const auto id = bus.register_listener(Loading::asset_upload_settled_event, [&](const Event& event) { events.push_back(event); });
    EXPECT_THROW(loader.upload(batch, provider), std::runtime_error);
    EXPECT_TRUE(Loading::FileRegistry::get().get_file_at(path));
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().outcome, Outcome::EffectsUnknown);
    EXPECT_EQ(events.front().counts.publications, 0u);
    EXPECT_EQ(events.front().counts.images_completed, 0u);
    EXPECT_TRUE(events.front().counts.publication_count_available);
    EXPECT_TRUE(bus.unregister_and_wait(id));
}

TEST(asset_upload_event, preparation_and_detach) {
    CE::Events::EventBus bus;
    Provider provider;
    const auto missing =
        std::filesystem::temp_directory_path() / ("missing-upload-preparation-" + std::to_string(provider.diagnostic_id()));
    ASSERT_FALSE(std::filesystem::exists(missing));
    Loading::Loader loader(missing);
    loader.publish_events_to(bus.publisher());
    int calls = 0;
    const auto id = bus.register_listener(Loading::asset_upload_settled_event, [&](const Event&) { ++calls; });
    EXPECT_THROW(static_cast<void>(loader.prepare()), CE::Exceptions::runtime_exception);
    EXPECT_EQ(calls, 0);
    loader.upload({}, provider);
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(bus.unregister_and_wait(id));
    loader.upload({}, provider);
    EXPECT_EQ(calls, 1);
}

TEST(asset_upload_event, reentrant_cutoff) {
    CE::Events::EventBus bus;
    Provider provider;
    Loading::Loader loader("unused-upload-root");
    loader.publish_events_to(bus.publisher());
    int first_calls = 0;
    bus.register_listener(Loading::asset_upload_settled_event, [&](const Event&) {
        if (++first_calls < 3)
            loader.upload({}, provider);
    });
    std::vector<std::uint64_t> attempts;
    const auto id =
        bus.register_listener(Loading::asset_upload_settled_event, [&](const Event& event) { attempts.push_back(event.attempt); });
    loader.upload({}, provider);
    ASSERT_EQ(attempts.size(), 1u);
    EXPECT_THROW(loader.publish_events_to(bus.publisher()), CE::Exceptions::failed_operation);
    loader.publish_pending_events();
    EXPECT_EQ(attempts.size(), 2u);
    loader.publish_pending_events();
    ASSERT_EQ(attempts.size(), 3u);
    EXPECT_LT(attempts[0], attempts[1]);
    EXPECT_LT(attempts[1], attempts[2]);
    EXPECT_TRUE(bus.unregister_and_wait(id));
}

TEST(asset_upload_event, bounded_pending) {
    CE::Events::EventBus bus;
    Provider provider;
    Loading::Loader loader("unused-upload-root");
    loader.publish_events_to(bus.publisher());
    bool create = true;
    bus.register_listener(Loading::asset_upload_settled_event, [&](const Event&) {
        if (std::exchange(create, false)) {
            for (int index = 0; index < 65; ++index)
                EXPECT_NO_THROW(loader.upload({}, provider));
        }
    });
    int calls = 0;
    const auto id = bus.register_listener(Loading::asset_upload_settled_event, [&](const Event&) { ++calls; });
    loader.upload({}, provider);
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(loader.diagnostics().completed);
    loader.publish_pending_events();
    EXPECT_EQ(calls, 65);
    EXPECT_TRUE(bus.unregister_and_wait(id));
}

TEST(asset_upload_event, expired_route) {
    Provider provider;
    Loading::Loader loader("unused-upload-root");
    {
        CE::Events::EventBus bus;
        loader.publish_events_to(bus.publisher());
    }
    EXPECT_NO_THROW(loader.upload({}, provider));
    EXPECT_TRUE(loader.diagnostics().completed);
}
