#include <cheryl/assets/resources/resource-provider.h>

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
    MemoryProvider provider;
    const CE::Assets::Images::Decoded pixels{{1, 1}, {255, 0, 0, 255}};
    const auto image = provider.create_image(pixels);
    return image->pixel_size().width == 1 && image->pixel_size().height == 1;
}
