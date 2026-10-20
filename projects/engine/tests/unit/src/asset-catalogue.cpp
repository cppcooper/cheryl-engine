#include <templates/asset-mgr.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <assets/resources/resource-provider.h>
#include <gtest/gtest.h>

#include <atomic>
#include <future>
#include <stdexcept>
#include <vector>

namespace {
    struct CopyKey {
        inline static bool reject_copy = false;
        std::string value;
        explicit CopyKey(std::string text)
        : value(std::move(text)) {}
        CopyKey(const CopyKey& other)
        : value(other.value) {
            if (reject_copy)
                throw std::runtime_error("Controlled observation-key copy failure");
        }
        CopyKey(CopyKey&&) noexcept = default;
        CopyKey& operator=(const CopyKey&) = default;
        CopyKey& operator=(CopyKey&&) noexcept = default;
        bool operator==(const CopyKey&) const = default;
    };
}
namespace std {
    template <> struct hash<CopyKey> {
        std::size_t operator()(const CopyKey& key) const { return std::hash<std::string>{}(key.value); }
    };
}

namespace {
    using CE::Assets::Cache::CatalogueChangeKind;
    using Change = CE::Assets::Cache::AssetCatalogueChanged<const int, std::string>;
    using Observation = CE::Assets::Cache::CatalogueObservation<const int, std::string>;
    const auto& channel = CE::Assets::Cache::asset_catalogue_changed_event<const int, std::string>;

    struct Cache final : CE::Assets::Cache::Store<const int, std::string> {
        bool metadata = false;
        Handle publish(std::string key, Handle candidate) {
            return publish_asset(key, std::move(candidate), [&]() noexcept { metadata = true; });
        }
        Handle replace(std::string key, Handle candidate) { return replace_asset(key, std::move(candidate)); }
        void clear_metadata() noexcept override { metadata = false; }
    };

    struct Image final : CE::Rendering::Resources::Image {
        CE::Rendering::Geometry::PixelSize pixel_size() const override { return {1, 1}; }
        void bind(std::uint32_t) const override {}
    };
    struct Provider final : CE::Rendering::Resources::Provider {
        CE::Rendering::GraphicsDomainOwner domain;
        bool supported = true;
        Provider() { domain.make_available(); }
        CE::Rendering::GraphicsDomain graphics_domain() const noexcept override {
            return supported ? domain.token() : CE::Rendering::GraphicsDomain{};
        }
        std::shared_ptr<CE::Rendering::Resources::Image> create_image(const CE::Assets::Images::Decoded&) override {
            return std::make_shared<Image>();
        }
        std::shared_ptr<CE::Rendering::Resources::Image>
        create_font_atlas(std::span<const unsigned char>, CE::Rendering::Geometry::PixelSize) override {
            return std::make_shared<Image>();
        }
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D>
        upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2D>, CE::Rendering::Geometry::Topology) override {
            throw std::logic_error("Unused geometry");
        }
        std::shared_ptr<CE::Rendering::Resources::Program> link_program(const std::vector<std::filesystem::path>&) override {
            throw std::logic_error("Unused program");
        }
    };
}

TEST(asset_catalogue, committed) {
    Cache cache;
    const auto source = cache.catalogue();
    std::vector<Observation> observations;
    auto watch = source.watch([&](const Observation& event) {
        if (event.change) {
            EXPECT_EQ(cache.metadata, event.change->kind != CatalogueChangeKind::Reset);
            const auto selected = cache.asset_snapshot("key"); // No callback under the cache lock.
            EXPECT_EQ(selected.revision, event.change->revision);
        }
        observations.push_back(event);
    });
    ASSERT_EQ(observations.size(), 1);
    ASSERT_TRUE(observations.front().seed);
    EXPECT_EQ(observations.front().seed->revision, 0);
    EXPECT_NE(observations.front().seed->source, 0);
    auto first = cache.publish("key", std::make_shared<const int>(10));
    const auto first_generation = cache.asset_snapshot("key").generation->generation;
    EXPECT_EQ(cache.publish("key", std::make_shared<const int>(20)), first);
    EXPECT_EQ(observations.size(), 2);
    auto second = cache.replace("key", std::make_shared<const int>(30));
    EXPECT_EQ(*first, 10);
    EXPECT_EQ(*second, 30);
    ASSERT_EQ(observations.size(), 3);
    const auto replacement = *observations.back().change;
    EXPECT_EQ(replacement.kind, CatalogueChangeKind::Replaced);
    EXPECT_EQ(replacement.previous->generation, first_generation);
    EXPECT_GT(replacement.current->generation, first_generation);
    EXPECT_EQ(replacement.key, "key");
    EXPECT_EQ(replacement.current->graphics_domain.snapshot().id, 0);
    cache.clear_assets();
    ASSERT_EQ(observations.size(), 4);
    EXPECT_EQ(observations.back().change->kind, CatalogueChangeKind::Reset);
    EXPECT_FALSE(observations.back().change->key);
    EXPECT_TRUE(source.snapshot().entries.empty());
    cache.clear_assets(); // Even an explicitly empty clear is a new Reset.
    EXPECT_EQ(observations.size(), 5);
    EXPECT_EQ(source.snapshot().revision, 4);
    watch.reset();
    EXPECT_FALSE(watch.attached());
    cache.publish("key", std::make_shared<const int>(40));
    EXPECT_GT(cache.asset_snapshot("key").generation->generation, replacement.current->generation);
    EXPECT_EQ(observations.size(), 5);
}

TEST(asset_catalogue, reentrant) {
    Cache cache;
    std::vector<std::uint64_t> revisions;
    int depth = 0;
    auto watch = cache.catalogue().watch([&](const Observation& observation) {
        if (!observation.change)
            return;
        EXPECT_EQ(++depth, 1);
        revisions.push_back(observation.change->revision);
        if (revisions.size() == 1)
            cache.publish("second", std::make_shared<const int>(2));
        --depth;
    });
    cache.publish("first", std::make_shared<const int>(1));
    EXPECT_EQ(revisions, (std::vector<std::uint64_t>{1, 2}));
    EXPECT_EQ(cache.catalogue().snapshot().pending, 0);
}

TEST(asset_catalogue, self_detach) {
    Cache cache;
    Cache::Catalogue::Watch watch;
    int changes = 0;
    watch = cache.catalogue().watch([&](const Observation& observation) {
        if (observation.change) {
            ++changes;
            watch.reset();
            EXPECT_EQ(cache.asset_snapshot("first").revision, 1);
        }
    });
    cache.publish("first", std::make_shared<const int>(1));
    cache.publish("second", std::make_shared<const int>(2));
    EXPECT_EQ(changes, 1);
    EXPECT_FALSE(watch.attached());
}

TEST(asset_catalogue, seed_commit) {
    Cache cache;
    std::vector<std::uint64_t> entered;
    auto watch = cache.catalogue().watch([&](const Observation& observation) {
        if (observation.seed) {
            entered.push_back(observation.seed->revision);
            cache.publish("key", std::make_shared<const int>(1));
        } else {
            entered.push_back(observation.change->revision);
        }
    });
    EXPECT_EQ(entered, (std::vector<std::uint64_t>{0, 1}));
    EXPECT_TRUE(watch.attached());
}

TEST(asset_catalogue, failure) {
    Cache cache;
    CE::Events::EventBus bus;
    cache.catalogue().publish_events_to(bus.publisher());
    const auto throwing = bus.register_listener(channel, [](const Change&) { throw std::runtime_error("observer failed"); });
    auto watch = cache.catalogue().watch([](const Observation& event) {
        if (event.change)
            throw std::runtime_error("optional watch failed");
    });
    std::shared_ptr<const int> result;
    EXPECT_NO_THROW(result = cache.replace("key", std::make_shared<const int>(1)));
    EXPECT_EQ(cache.get_asset("key"), result);
    EXPECT_EQ(cache.catalogue().snapshot().revision, 1);
    EXPECT_EQ(cache.catalogue().snapshot().pending, 0);
    throwing.disconnect();
    EXPECT_THROW(
        static_cast<void>(cache.catalogue().watch([](const Observation&) { throw std::runtime_error("seed failed"); })), std::runtime_error
    );
    EXPECT_NO_THROW(cache.clear_assets());
}

TEST(asset_catalogue, payload_failure) {
    struct Keys : CE::Assets::Cache::Store<const int, CopyKey> {
        Handle replace(const CopyKey& key, Handle candidate) { return replace_asset(key, std::move(candidate)); }
    } cache;
    CE::Events::EventBus bus;
    cache.catalogue().publish_events_to(bus.publisher());
    int attempts = 0;
    const auto registration = bus.register_listener(
        CE::Assets::Cache::asset_catalogue_changed_event<const int, CopyKey>,
        [&](const CE::Assets::Cache::AssetCatalogueChanged<const int, CopyKey>&) { ++attempts; }
    );
    const CopyKey key("key");
    const auto old = cache.replace(key, std::make_shared<const int>(10));
    CopyKey::reject_copy = true;
    std::shared_ptr<const int> result;
    EXPECT_NO_THROW(result = cache.replace(key, std::make_shared<const int>(20)));
    CopyKey::reject_copy = false;
    EXPECT_EQ(*old, 10);
    EXPECT_EQ(result, cache.get_asset(key));
    EXPECT_EQ(*result, 20);
    EXPECT_EQ(cache.catalogue().snapshot().revision, 2);
    EXPECT_EQ(cache.catalogue().snapshot().lost_observations, 1);
    EXPECT_EQ(cache.catalogue().snapshot().last_lost_revision, 2);
    EXPECT_EQ(attempts, 1);
    registration.disconnect();
}

TEST(asset_catalogue, retained_release) {
    Cache cache;
    bool released = false;
    cache.publish("key", std::shared_ptr<const int>(new int{10}, [&](const int* value) {
                      EXPECT_EQ(*cache.get_asset("key"), 20);
                      released = true;
                      delete value;
                  }));
    auto watch = cache.catalogue().watch([&](const Observation& event) {
        if (event.change && event.change->kind == CatalogueChangeKind::Replaced)
            EXPECT_TRUE(released);
    });
    cache.replace("key", std::make_shared<const int>(20));
    EXPECT_TRUE(released);
}

TEST(asset_catalogue, concurrent_seed) {
    Cache cache;
    auto initial_watch = cache.catalogue().watch([](const Observation&) {});
    CE::Events::EventBus bus;
    cache.catalogue().publish_events_to(bus.publisher());
    std::promise<void> entered;
    std::promise<void> release;
    const auto released = release.get_future().share();
    const auto blocking = bus.register_listener(channel, [&](const Change& change) {
        if (change.revision == 1) {
            entered.set_value();
            released.wait();
        }
    });
    auto producer = std::async(std::launch::async, [&] { cache.publish("first", std::make_shared<const int>(1)); });
    entered.get_future().wait();
    cache.publish("second", std::make_shared<const int>(2));
    std::vector<Observation> observations;
    auto watch = cache.catalogue().watch([&](const Observation& event) { observations.push_back(event); });
    ASSERT_EQ(observations.size(), 1);
    EXPECT_EQ(observations.front().seed->revision, 2);
    EXPECT_EQ(observations.front().seed->entries.size(), 2);
    release.set_value();
    producer.get();
    EXPECT_EQ(observations.size(), 1); // Delayed commits at/below the seed are filtered.
    cache.publish("third", std::make_shared<const int>(3));
    ASSERT_EQ(observations.size(), 2);
    EXPECT_EQ(observations.back().change->revision, 3);
    blocking.disconnect();
}

TEST(asset_catalogue, bounded) {
    Cache cache;
    CE::Events::EventBus bus;
    cache.catalogue().publish_events_to(bus.publisher());
    std::promise<void> entered;
    std::promise<void> release;
    const auto released = release.get_future().share();
    std::vector<Change> records;
    const auto observer = bus.register_listener(channel, [&](const Change& change) {
        records.push_back(change);
        if (change.revision == 1) {
            entered.set_value();
            released.wait();
        }
    });
    auto first = std::async(std::launch::async, [&] { cache.publish("key", std::make_shared<const int>(0)); });
    entered.get_future().wait();
    for (int index = 1; index <= 100; ++index)
        cache.replace("key", std::make_shared<const int>(index));
    EXPECT_EQ(cache.catalogue().snapshot().pending, 64);
    EXPECT_EQ(cache.catalogue().snapshot().lost_observations, 36);
    EXPECT_EQ(*cache.get_asset("key"), 100);
    release.set_value();
    first.get();
    EXPECT_EQ(records.size(), 64); // One drain attempts at most 64, including its first record.
    EXPECT_EQ(cache.catalogue().snapshot().pending, 1);
    cache.catalogue().publish_notifications();
    ASSERT_EQ(records.size(), 65);
    EXPECT_EQ(records.front().revision, 1);
    EXPECT_EQ(records[1].revision, 38);
    EXPECT_EQ(records.back().revision, 101);
    EXPECT_EQ(records.back().lost_observations, 36);
    EXPECT_EQ(records.back().first_lost_revision, 2);
    EXPECT_EQ(records.back().last_lost_revision, 37);
    EXPECT_EQ(cache.catalogue().snapshot().pending, 0);
    observer.disconnect();
}

TEST(asset_catalogue, namespace_keys) {
    Cache strings;
    struct Paths : CE::Assets::Cache::Store<const int> {
        Handle publish(const std::filesystem::path& key) { return publish_asset(key, std::make_shared<const int>(1)); }
    } paths;
    CE::Events::EventBus bus;
    strings.catalogue().publish_events_to(bus.publisher());
    paths.catalogue().publish_events_to(bus.publisher());
    int string_events = 0;
    int path_events = 0;
    const auto string_registration = bus.register_listener(channel, [&](const Change&) { ++string_events; });
    const auto path_registration = bus.register_listener(
        CE::Assets::Cache::asset_catalogue_changed_event<const int, std::filesystem::path>,
        [&](const CE::Assets::Cache::AssetCatalogueChanged<const int, std::filesystem::path>& event) {
            ++path_events;
            EXPECT_EQ(event.key, std::filesystem::path("a/../b")); // Base cache does not normalize keys.
        }
    );
    strings.publish("a/../b", std::make_shared<const int>(2));
    paths.publish("a/../b");
    EXPECT_EQ(string_events, 1);
    EXPECT_EQ(path_events, 1);
    string_registration.disconnect();
    path_registration.disconnect();
}

TEST(asset_catalogue, destruction) {
    CE::Events::EventBus bus;
    std::vector<Change> records;
    const auto registration = bus.register_listener(channel, [&](const Change& change) { records.push_back(change); });
    auto cache = std::make_unique<Cache>();
    auto source = cache->catalogue();
    source.publish_events_to(bus.publisher());
    auto watch = source.watch([](const Observation&) {});
    const auto retained = cache->publish("key", std::make_shared<const int>(10));
    cache.reset();
    EXPECT_EQ(records.size(), 1); // Destruction is closure, never Reset.
    EXPECT_TRUE(source.snapshot().closed);
    EXPECT_TRUE(source.snapshot().entries.empty());
    EXPECT_FALSE(watch.attached());
    EXPECT_EQ(*retained, 10);
    EXPECT_THROW(static_cast<void>(source.watch([](const Observation&) {})), CE::Exceptions::bad_request);
    source.publish_notifications();
    EXPECT_EQ(records.size(), 1);
    registration.disconnect();
}

TEST(asset_catalogue, callback_destruction) {
    CE::Events::EventBus bus;
    auto cache = std::make_unique<Cache>();
    const auto source = cache->catalogue();
    source.publish_events_to(bus.publisher());
    int local_changes = 0;
    auto watch = source.watch([&](const Observation& event) {
        if (event.change)
            ++local_changes;
    });
    const auto destroying = bus.register_listener(channel, [&](const Change&) { cache.reset(); });
    auto retained = cache->publish("key", std::make_shared<const int>(10));
    EXPECT_FALSE(cache);
    EXPECT_EQ(*retained, 10);
    EXPECT_TRUE(source.snapshot().closed);
    EXPECT_EQ(local_changes, 0); // Source closure prevents a not-yet-entered local callback.
    destroying.disconnect();
}

TEST(asset_catalogue, provenance) {
    using ImageChange = CE::Assets::Cache::AssetCatalogueChanged<CE::Rendering::Resources::Image, std::filesystem::path>;
    CE::Events::EventBus bus;
    std::vector<ImageChange> records;
    const auto registration = bus.register_listener(
        CE::Assets::Cache::asset_catalogue_changed_event<CE::Rendering::Resources::Image, std::filesystem::path>,
        [&](const ImageChange& change) { records.push_back(change); }
    );
    CE::Rendering::GraphicsDomain retained_domain;
    auto& images = CE::Assets::Images::Manager::get();
    {
        Provider provider;
        provider.publish_events_to(bus.publisher());
        retained_domain = provider.graphics_domain();
        images.upload("catalogue-domain", CE::Assets::Images::Decoded{{1, 1}, {255, 255, 255, 255}}, provider);
        const auto selected = images.asset_snapshot("catalogue-domain");
        ASSERT_TRUE(selected.generation);
        EXPECT_EQ(selected.generation->graphics_domain.snapshot(), retained_domain.snapshot());
        ASSERT_EQ(records.size(), 1);
        EXPECT_NE(records[0].source, retained_domain.snapshot().id);
        EXPECT_EQ(records[0].current->graphics_domain.snapshot(), retained_domain.snapshot());
        provider.domain.shutdown();
        const CE::Rendering::GraphicsDomainStateChanged terminal{retained_domain.snapshot(), retained_domain};
        EXPECT_TRUE(CE::Assets::Cache::Context::evict_graphics_domain(terminal));
        ASSERT_EQ(records.size(), 2);
        EXPECT_EQ(records.back().kind, CatalogueChangeKind::Reset);
        EXPECT_TRUE(images.catalogue().snapshot().entries.empty());
        EXPECT_EQ(selected.generation->graphics_domain.snapshot().sequence, 2);
        EXPECT_EQ(selected.asset->pixel_size().width, 1);
    }
    {
        Provider provider;
        provider.supported = false;
        provider.publish_events_to(bus.publisher());
        images.upload("unsupported-domain", CE::Assets::Images::Decoded{{1, 1}, {255, 255, 255, 255}}, provider);
        EXPECT_EQ(images.asset_snapshot("unsupported-domain").generation->graphics_domain.snapshot().id, 0);
    }
    EXPECT_EQ(records.size(), 3); // Provider destructor cleanup invokes no Reset/listener.
    EXPECT_TRUE(images.catalogue().snapshot().entries.empty());
    registration.disconnect();
}
