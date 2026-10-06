#include <ui/rmlui/rendering.h>

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Matrix4.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <utility>

namespace CE::UI::RmlUi {
    namespace {
        std::uintptr_t next_handle(std::uintptr_t& next) {
            if (next == 0)
                throw Exceptions::failed_operation(CE_HERE, "RmlUi CPU handle space is exhausted");
            return next++;
        }
    }

    RenderTarget::RenderTarget(const unsigned int maximum_texture_size)
    : maximum_texture_size_(maximum_texture_size) {
        if (Rml::GetVersion() != "6.3")
            throw Exceptions::failed_operation(CE_HERE, "Cheryl UI requires RmlUi 6.3");
        if (maximum_texture_size == 0 || maximum_texture_size > static_cast<unsigned int>(std::numeric_limits<int>::max()))
            throw Exceptions::invalid_args(CE_HERE, "RmlUi needs a positive, representable texture bound");
    }

    void RenderTarget::set_view(const Rml::Vector2i logical_size) {
        if (recording_)
            throw Exceptions::failed_operation(CE_HERE, "Cannot change the RmlUi view during a recording");
        if (logical_size.x < 0 || logical_size.y < 0)
            throw Exceptions::invalid_args(CE_HERE, "RmlUi logical dimensions must be nonnegative");
        size_ = logical_size;
        configured_ = true;
    }

    void RenderTarget::begin_recording() {
        if (!configured_ || recording_)
            throw Exceptions::failed_operation(CE_HERE, "RmlUi recording needs a configured view and no active recording");
        if (unsupported_)
            throw Exceptions::failed_operation(CE_HERE, unsupported_);
        draws_.clear();
        scissor_ = {0, 0, static_cast<double>(size_.x), static_cast<double>(size_.y)};
        scissoring_ = false;
        recording_ = true;
    }

    RecordedScene RenderTarget::finish_recording() {
        if (!recording_)
            throw Exceptions::failed_operation(CE_HERE, "No RmlUi recording is active");
        if (unsupported_) {
            discard_recording();
            throw Exceptions::failed_operation(CE_HERE, unsupported_);
        }
        RecordedScene result;
        result.width_ = static_cast<float>(size_.x);
        result.height_ = static_cast<float>(size_.y);
        result.draws_ = std::move(draws_);
        recording_ = false;
        return result;
    }

    void RenderTarget::discard_recording() noexcept {
        draws_.clear();
        recording_ = false;
    }

    Rml::CompiledGeometryHandle
    RenderTarget::CompileGeometry(const Rml::Span<const Rml::Vertex> vertices, const Rml::Span<const int> indices) {
        if (indices.empty())
            return 0;
        if (indices.size() % 3 != 0 || vertices.empty())
            throw Exceptions::invalid_args(CE_HERE, "RmlUi geometry must contain indexed triangles");
        std::vector<Vertex2DColor> copy;
        copy.reserve(indices.size());
        for (const int index : indices) {
            if (index < 0 || static_cast<std::size_t>(index) >= vertices.size())
                throw Exceptions::invalid_args(CE_HERE, "RmlUi geometry index is outside its vertices");
            const auto& vertex = vertices[static_cast<std::size_t>(index)];
            if (!std::isfinite(vertex.position.x) || !std::isfinite(vertex.position.y) || !std::isfinite(vertex.tex_coord.x) ||
                !std::isfinite(vertex.tex_coord.y))
                throw Exceptions::invalid_args(CE_HERE, "RmlUi vertices need finite positions and texture coordinates");
            copy.push_back(
                {vertex.position.x, vertex.position.y, 0, vertex.tex_coord.x, 1 - vertex.tex_coord.y, vertex.colour.red / 255.0f,
                 vertex.colour.green / 255.0f, vertex.colour.blue / 255.0f, vertex.colour.alpha / 255.0f}
            );
        }
        const auto handle = next_handle(next_geometry_);
        geometry_.emplace(handle, std::move(copy));
        return handle;
    }

    void RenderTarget::RenderGeometry(
        const Rml::CompiledGeometryHandle geometry,
        const Rml::Vector2f translation,
        const Rml::TextureHandle texture
    ) {
        if (!recording_)
            throw Exceptions::failed_operation(CE_HERE, "RmlUi draws require an active recording");
        const auto found = geometry_.find(geometry);
        if (found == geometry_.end() || !std::isfinite(translation.x) || !std::isfinite(translation.y))
            throw Exceptions::invalid_args(CE_HERE, "RmlUi draw needs live geometry and a finite translation");
        RecordedDraw draw;
        if (texture != 0) {
            const auto image = textures_.find(texture);
            if (image == textures_.end())
                throw Exceptions::invalid_args(CE_HERE, "RmlUi draw references a released or foreign texture");
            draw.texture = image->second;
        }
        const RenderAPIs::ClipRect2D viewport{0, 0, static_cast<double>(size_.x), static_cast<double>(size_.y)};
        draw.clip = {scissoring_ ? RenderAPIs::intersect_clip_rects(scissor_, viewport) : viewport, static_cast<double>(size_.x),
                     static_cast<double>(size_.y)};
        if (draw.clip.rectangle.left == draw.clip.rectangle.right || draw.clip.rectangle.top == draw.clip.rectangle.bottom)
            return;
        draw.vertices = found->second;
        for (auto& vertex : draw.vertices) {
            vertex.x += translation.x;
            vertex.y += translation.y;
            if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y))
                throw Exceptions::invalid_args(CE_HERE, "RmlUi translated positions are not representable");
            if (draw.texture && (vertex.u < 0 || vertex.u > 1 || vertex.v < 0 || vertex.v > 1))
                reject("RmlUi repeating texture coordinates require a new sampler contract");
        }
        draws_.push_back(std::move(draw));
    }

    void RenderTarget::ReleaseGeometry(const Rml::CompiledGeometryHandle geometry) {
        geometry_.erase(geometry);
    }

    std::size_t RenderTarget::texture_bytes(const Rml::Vector2i dimensions) const {
        if (dimensions.x <= 0 || dimensions.y <= 0 || static_cast<unsigned int>(dimensions.x) > maximum_texture_size_ ||
            static_cast<unsigned int>(dimensions.y) > maximum_texture_size_)
            throw Exceptions::invalid_args(CE_HERE, "RmlUi texture dimensions exceed the configured bound");
        const auto width = static_cast<std::size_t>(dimensions.x);
        const auto height = static_cast<std::size_t>(dimensions.y);
        if (width > std::numeric_limits<std::size_t>::max() / 4 || height > std::numeric_limits<std::size_t>::max() / (width * 4))
            throw Exceptions::invalid_args(CE_HERE, "RmlUi RGBA byte count overflows");
        return width * height * 4;
    }

    Rml::TextureHandle RenderTarget::store_texture(Assets::DecodedImage image) {
        const auto handle = next_handle(next_texture_);
        textures_.emplace(handle, std::make_shared<const Assets::DecodedImage>(std::move(image)));
        return handle;
    }

    Rml::TextureHandle RenderTarget::LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) {
        dimensions = {};
        Assets::DecodedImage image;
        try {
            image = Assets::decode_image(std::filesystem::u8path(source));
        } catch (const std::exception&) {
            return 0; // The toolkit reports an unavailable file image.
        }
        if (image.size.width > maximum_texture_size_ || image.size.height > maximum_texture_size_)
            return 0;
        const Rml::Vector2i size{static_cast<int>(image.size.width), static_cast<int>(image.size.height)};
        if (image.rgba.size() != texture_bytes(size))
            throw Exceptions::failed_operation(CE_HERE, "Decoded RmlUi image has incompatible RGBA metadata");
        for (std::size_t i = 0; i < image.rgba.size(); i += 4) {
            const auto alpha = image.rgba[i + 3];
            for (std::size_t channel = 0; channel < 3; ++channel)
                image.rgba[i + channel] = static_cast<unsigned char>((image.rgba[i + channel] * alpha + 127) / 255);
        }
        const auto handle = store_texture(std::move(image));
        dimensions = size;
        return handle;
    }

    Rml::TextureHandle RenderTarget::GenerateTexture(const Rml::Span<const Rml::byte> pixels, const Rml::Vector2i dimensions) {
        const auto bytes = texture_bytes(dimensions);
        if (pixels.size() != bytes)
            throw Exceptions::invalid_args(CE_HERE, "RmlUi RGBA pixels do not match their dimensions");
        Assets::DecodedImage image;
        image.size = {static_cast<unsigned int>(dimensions.x), static_cast<unsigned int>(dimensions.y)};
        image.rgba.assign(pixels.begin(), pixels.end());
        return store_texture(std::move(image));
    }

    void RenderTarget::ReleaseTexture(const Rml::TextureHandle texture) {
        textures_.erase(texture);
    }

    void RenderTarget::EnableScissorRegion(const bool enable) {
        scissoring_ = enable;
    }

    void RenderTarget::SetScissorRegion(const Rml::Rectanglei region) {
        if (!region.Valid())
            throw Exceptions::invalid_args(CE_HERE, "RmlUi scissor edges are inverted");
        scissor_ = {static_cast<double>(region.Left()), static_cast<double>(region.Top()), static_cast<double>(region.Right()),
                    static_cast<double>(region.Bottom())};
    }

    void RenderTarget::reject(const char* feature) noexcept {
        if (!unsupported_)
            unsupported_ = feature;
    }

    void RenderTarget::EnableClipMask(const bool enable) {
        if (enable)
            reject("RmlUi clip masks are unavailable; use rectangular clipping");
    }
    void RenderTarget::RenderToClipMask(Rml::ClipMaskOperation, Rml::CompiledGeometryHandle, Rml::Vector2f) {
        reject("RmlUi clip masks are unavailable; use rectangular clipping");
    }
    void RenderTarget::SetTransform(const Rml::Matrix4f* transform) {
        if (transform && *transform != Rml::Matrix4f::Identity())
            reject("RmlUi transformed elements require a transform/clipping contract");
    }
    Rml::LayerHandle RenderTarget::PushLayer() {
        reject("RmlUi offscreen layers are unavailable");
        return 0;
    }
    void RenderTarget::CompositeLayers(Rml::LayerHandle, Rml::LayerHandle, Rml::BlendMode, Rml::Span<const Rml::CompiledFilterHandle>) {
        reject("RmlUi offscreen layers are unavailable");
    }
    void RenderTarget::PopLayer() {
        reject("RmlUi offscreen layers are unavailable");
    }
    Rml::TextureHandle RenderTarget::SaveLayerAsTexture() {
        reject("RmlUi offscreen textures are unavailable");
        return 0;
    }
    Rml::CompiledFilterHandle RenderTarget::SaveLayerAsMaskImage() {
        reject("RmlUi mask images are unavailable");
        return 0;
    }
    Rml::CompiledFilterHandle RenderTarget::CompileFilter(const Rml::String&, const Rml::Dictionary&) {
        reject("RmlUi filters are unavailable");
        return 0;
    }
    void RenderTarget::ReleaseFilter(Rml::CompiledFilterHandle) {}
    Rml::CompiledShaderHandle RenderTarget::CompileShader(const Rml::String&, const Rml::Dictionary&) {
        reject("RmlUi custom shaders are unavailable");
        return 0;
    }
    void RenderTarget::RenderShader(Rml::CompiledShaderHandle, Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) {
        reject("RmlUi custom shaders are unavailable");
    }
    void RenderTarget::ReleaseShader(Rml::CompiledShaderHandle) {}
}
