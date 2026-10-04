#include <cheryl/assets/resources/resource-provider.h>

#include <memory>
#include <stdexcept>

#if defined(GL_VERSION_3_3) || defined(GLFW_VERSION_MAJOR)
#error Generic resource headers must not include OpenGL or GLFW.
#endif

namespace {
    class MemoryImage final : public CE::Assets::Image {
        const CE::Assets::PixelSize size_;

    public:
        explicit MemoryImage(CE::Assets::PixelSize size)
        : size_(size) {}
        CE::Assets::PixelSize pixel_size() const override { return size_; }
        void bind(std::uint32_t) const override {}
    };

    class MemoryProvider final : public CE::Assets::ResourceProvider {
    public:
        std::shared_ptr<CE::Assets::Image> create_image(const CE::Assets::DecodedImage& image) override {
            return std::make_shared<MemoryImage>(image.size);
        }
        std::shared_ptr<CE::Assets::Image> create_font_atlas(std::span<const unsigned char>, CE::Assets::PixelSize size) override {
            return std::make_shared<MemoryImage>(size);
        }
        std::shared_ptr<CE::Assets::Geometry2D>
        upload_geometry(std::span<const CE::Vertex2D>, CE::Assets::PrimitiveTopology) override {
            throw std::logic_error("The consumer only creates images");
        }
        std::shared_ptr<CE::Assets::Shader> link_program(const std::vector<std::filesystem::path>&) override {
            throw std::logic_error("The consumer only creates images");
        }
    };
}

bool exercise_resources() {
    MemoryProvider provider;
    const CE::Assets::DecodedImage pixels{{1, 1}, {255, 0, 0, 255}};
    const auto image = provider.create_image(pixels);
    return image->pixel_size().width == 1 && image->pixel_size().height == 1;
}
