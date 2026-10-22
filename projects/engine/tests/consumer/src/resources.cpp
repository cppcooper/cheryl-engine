#include <cheryl/assets/resources/resource-provider.h>
#include <cheryl/core/resources/asset-management/asset-loader.h>
#include <cheryl/core/resources/asset-management/texture-mgr.h>

#include <memory>
#include <stdexcept>

#if defined(GL_VERSION_3_3) || defined(GLFW_VERSION_MAJOR)
#error Generic resource headers must not include OpenGL or GLFW.
#endif

namespace {
    class MemoryImage final : public CE::Rendering::Resources::Image {
        const CE::Rendering::Geometry::PixelSize size_;

    public:
        explicit MemoryImage(CE::Rendering::Geometry::PixelSize size)
        : size_(size) {}
        CE::Rendering::Geometry::PixelSize pixel_size() const override { return size_; }
        void bind(std::uint32_t) const override {}
    };

    class MemoryProvider final : public CE::Rendering::Resources::Provider {
    public:
        std::shared_ptr<CE::Rendering::Resources::Image> create_image(const CE::Assets::Images::Decoded& image) override {
            return std::make_shared<MemoryImage>(image.size);
        }
        std::shared_ptr<CE::Rendering::Resources::Image>
        create_font_atlas(std::span<const unsigned char>, CE::Rendering::Geometry::PixelSize size) override {
            return std::make_shared<MemoryImage>(size);
        }
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D>
        upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2D>, CE::Rendering::Geometry::Topology) override {
            throw std::logic_error("The consumer only creates images");
        }
        std::shared_ptr<CE::Rendering::Resources::Program> link_program(const std::vector<std::filesystem::path>&) override {
            throw std::logic_error("The consumer only creates images");
        }
    };
}

bool exercise_resources() {
    CE::Rendering::GraphicsDomain retained;
    {
        CE::Rendering::GraphicsDomainOwner owner;
        retained = owner.token();
        owner.make_available();
        owner.shutdown();
    }
    if (retained.snapshot().reason != CE::Rendering::GraphicsDomainReason::Shutdown)
        return false;
    MemoryProvider provider;
    const CE::Assets::Images::Decoded pixels{{1, 1}, {255, 0, 0, 255}};
    const auto image = provider.create_image(pixels);
    CE::Events::EventBus events;
    provider.publish_events_to(events.publisher());
    CE::Assets::Loading::Loader loader("consumer-upload");
    loader.publish_events_to(events.publisher());
    bool settled = false;
    const auto registration = events.register_listener(
        CE::Assets::Loading::asset_upload_settled_event,
        [&](const CE::Assets::Loading::AssetUploadSettled& event) {
            settled = event.source == loader.source_id() && event.attempt != 0 &&
                      event.outcome == CE::Assets::Loading::UploadOutcome::Succeeded;
        }
    );
    loader.upload({}, provider);
    events.unregister_and_wait(registration);
    auto& images = CE::Assets::Images::Manager::get();
    bool published = false;
    const auto catalogue_registration = events.register_listener(
        CE::Assets::Cache::asset_catalogue_changed_event<CE::Rendering::Resources::Image, std::filesystem::path>,
        [&](const CE::Assets::Cache::AssetCatalogueChanged<CE::Rendering::Resources::Image, std::filesystem::path>& change) {
            const auto current = images.asset_snapshot("consumer-image");
            published = change.kind == CE::Assets::Cache::CatalogueChangeKind::Published &&
                change.key == "consumer-image" && current.asset && current.generation &&
                current.generation->generation == change.current->generation &&
                change.current->graphics_domain.snapshot().id == 0;
        });
    images.upload("consumer-image", pixels, provider);
    catalogue_registration.disconnect();
    return image->pixel_size().width == 1 && image->pixel_size().height == 1 && settled && published;
}
