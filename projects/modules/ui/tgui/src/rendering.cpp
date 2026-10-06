#include <ui/tgui/rendering.h>

#include "dependency-contract.h"

#include <TGUI/Container.hpp>
#include <internals/exceptions.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace CE::UI::TGUI {
    namespace {
        void validate_bound(const unsigned int bound) {
            if (bound == 0 || bound > static_cast<unsigned int>(std::numeric_limits<int>::max()))
                throw Exceptions::invalid_args(CE_HERE, "TGUI needs a positive, representable configured texture bound");
        }

        void validate_rectangle(const tgui::FloatRect& rect) {
            if (!std::isfinite(rect.left) || !std::isfinite(rect.top) || !std::isfinite(rect.width) || !std::isfinite(rect.height) ||
                !std::isfinite(rect.left + rect.width) || !std::isfinite(rect.top + rect.height) || rect.width < 0 || rect.height < 0)
                throw Exceptions::invalid_args(CE_HERE, "TGUI rectangles need finite, ordered edges");
        }

        void validate_transform(const tgui::Transform& transform) {
            const auto& matrix = transform.getMatrix();
            if (!std::all_of(matrix.begin(), matrix.end(), [](const float value) { return std::isfinite(value); }))
                throw Exceptions::invalid_args(CE_HERE, "TGUI drawing needs a finite transform");
        }

        tgui::Vector2f pixel_rounding(
            const tgui::FloatRect view,
            const tgui::FloatRect viewport,
            const tgui::Vector2f target,
            const tgui::Vector2f scale
        ) {
            if (target.x == 0 || target.y == 0 || view.width == 0 || view.height == 0 || viewport.width == 0 || viewport.height == 0)
                return {1, 1};
            const tgui::Vector2f result{scale.x * (viewport.width / view.width), scale.y * (viewport.height / view.height)};
            if (!std::isfinite(result.x) || !std::isfinite(result.y) || result.x <= 0 || result.y <= 0)
                throw Exceptions::invalid_args(CE_HERE, "TGUI view-to-pixel scale is not representable");
            return result;
        }

        RenderAPIs::ClipRect2D viewport_rectangle(const tgui::FloatRect& viewport) {
            return {viewport.left, viewport.top, static_cast<double>(viewport.left) + viewport.width,
                    static_cast<double>(viewport.top) + viewport.height};
        }

        RenderAPIs::ClipRegion2D clip_region(const RenderAPIs::ClipRect2D& rectangle, const tgui::Vector2f size) {
            RenderAPIs::ClipRegion2D clip{rectangle, size.x, size.y};
            RenderAPIs::validate_clip_region(clip);
            return clip;
        }
    }

    Texture::Texture(const unsigned int maximum_size)
    : maximum_size_(maximum_size) {
        validate_bound(maximum_size);
    }

    bool Texture::loadTextureOnly(const tgui::Vector2u size, const std::uint8_t* pixels, const bool smooth) {
        if (!smooth)
            throw Exceptions::invalid_args(CE_HERE, "TGUI nearest sampling is not supported by the current provider contract");
        const auto width = static_cast<std::size_t>(size.x);
        const auto height = static_cast<std::size_t>(size.y);
        if (!pixels || width == 0 || height == 0 || width > maximum_size_ || height > maximum_size_ ||
            width > std::numeric_limits<std::size_t>::max() / 4 || height > std::numeric_limits<std::size_t>::max() / (width * 4))
            throw Exceptions::invalid_args(CE_HERE, "TGUI RGBA pixels need nonempty dimensions within the configured bound");
        const auto byte_count = width * height * 4;
        if (byte_count > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()))
            throw Exceptions::invalid_args(CE_HERE, "TGUI RGBA pixels exceed addressable storage");
        auto copy = std::make_shared<Assets::DecodedImage>();
        copy->size = {size.x, size.y};
        copy->rgba.assign(pixels, pixels + byte_count);
        // Base load() may retain its own pixels for hit testing. Font atlases use
        // loadTextureOnly() with transient storage; this snapshot owns both paths.
        tgui::BackendTexture::loadTextureOnly(size, pixels, smooth);
        snapshot_ = std::move(copy);
        return true;
    }

    void Texture::setSmooth(const bool smooth) {
        if (!smooth)
            throw Exceptions::invalid_args(CE_HERE, "TGUI nearest sampling requires a new provider sampling contract");
        tgui::BackendTexture::setSmooth(smooth);
    }

    Renderer::Renderer(const unsigned int maximum_size)
    : maximum_size_(maximum_size) {
        validate_bound(maximum_size);
    }

    std::shared_ptr<tgui::BackendTexture> Renderer::createTexture() {
        return std::make_shared<Texture>(maximum_size_);
    }

    void RenderTarget::setView(const tgui::FloatRect view, const tgui::FloatRect viewport, const tgui::Vector2f targetSize) {
        if (recording_)
            throw Exceptions::failed_operation(CE_HERE, "Cannot change the TGUI view during a recording");
        validate_rectangle(view);
        validate_rectangle(viewport);
        if (!std::isfinite(targetSize.x) || !std::isfinite(targetSize.y) || targetSize.x < 0 || targetSize.y < 0)
            throw Exceptions::invalid_args(CE_HERE, "TGUI target dimensions must be finite and nonnegative");
        const auto rounding = pixel_rounding(view, viewport, targetSize, pixel_scale_);
        tgui::BackendRenderTarget::setView(view, viewport, targetSize);
        configured_ = true;
        clip_rectangle_ = viewport_rectangle(viewport);
        m_pixelsPerPoint = rounding;
    }

    void RenderTarget::set_pixel_scale(const tgui::Vector2f scale) {
        if (recording_)
            throw Exceptions::failed_operation(CE_HERE, "Cannot change TGUI pixel scale during a recording");
        if (!std::isfinite(scale.x) || !std::isfinite(scale.y) || scale.x <= 0 || scale.y <= 0)
            throw Exceptions::invalid_args(CE_HERE, "TGUI pixel scale must be finite and positive");
        const auto rounding = pixel_rounding(m_viewRect, m_viewport, m_targetSize, scale);
        pixel_scale_ = scale;
        m_pixelsPerPoint = rounding;
    }

    bool RenderTarget::drawable() const {
        return m_targetSize.x > 0 && m_targetSize.y > 0 && m_viewRect.width > 0 && m_viewRect.height > 0 && m_viewport.width > 0 &&
               m_viewport.height > 0;
    }

    void RenderTarget::require_recording() const {
        if (!recording_)
            throw Exceptions::failed_operation(CE_HERE, "TGUI drawing needs an active recording");
    }

    void RenderTarget::begin_recording() {
        if (!configured_ || recording_)
            throw Exceptions::failed_operation(CE_HERE, "TGUI recording needs a configured target with no active recording");
        draws_.clear();
        clip_rectangle_ = viewport_rectangle(m_viewport);
        recording_ = true;
    }

    RecordedScene RenderTarget::finish_recording() {
        require_recording();
        if (!m_clipLayers.empty())
            throw Exceptions::failed_operation(CE_HERE, "TGUI recording has unmatched clipping layers");
        RecordedScene scene;
        scene.width_ = m_targetSize.x;
        scene.height_ = m_targetSize.y;
        scene.draws_ = std::move(draws_);
        recording_ = false;
        return scene;
    }

    void RenderTarget::discard_recording() noexcept {
        draws_.clear();
        m_clipLayers.clear();
        clip_rectangle_ = viewport_rectangle(m_viewport);
        recording_ = false;
    }

    void RenderTarget::drawGui(const std::shared_ptr<tgui::RootContainer>& root) {
        if (!root)
            throw Exceptions::invalid_args(CE_HERE, "TGUI drawing needs a root container");
        begin_recording();
        try {
            if (drawable())
                root->draw(*this, {});
            if (!m_clipLayers.empty())
                throw Exceptions::failed_operation(CE_HERE, "TGUI widget left unmatched clipping layers");
        } catch (...) {
            discard_recording();
            throw;
        }
    }

    void RenderTarget::addClippingLayer(const tgui::RenderStates& states, const tgui::FloatRect rect) {
        require_recording();
        validate_rectangle(rect);
        validate_transform(states.transform);
        validate_rectangle(states.transform.transformRect(rect));
        // Match the rotations supported by the pinned toolkit's rectangular
        // clipping algorithm. It silently keeps the old clip for other rotations.
        const auto& matrix = states.transform.getMatrix();
        const auto near = [](const float value, const float expected) { return std::abs(value - expected) <= 0.00001f; };
        if (!(near(matrix[1], 0) && near(matrix[4], 0)) && !(near(matrix[1], 1) && near(matrix[4], -1)) &&
            !(near(matrix[1], -1) && near(matrix[4], 1)))
            throw Exceptions::invalid_args(CE_HERE, "TGUI rotated clipping requires a mask contract");
        if (!drawable()) {
            m_clipLayers.emplace_back(tgui::FloatRect{}, tgui::FloatRect{});
            clip_rectangle_ = {};
            return;
        }
        const auto old_size = m_clipLayers.size();
        const auto old_clip = clip_rectangle_;
        try {
            tgui::BackendRenderTarget::addClippingLayer(states, rect);
        } catch (...) {
            m_clipLayers.resize(old_size);
            clip_rectangle_ = old_clip;
            throw;
        }
    }

    void RenderTarget::removeClippingLayer() {
        require_recording();
        if (m_clipLayers.empty())
            throw Exceptions::failed_operation(CE_HERE, "TGUI clipping stack is empty");
        tgui::BackendRenderTarget::removeClippingLayer();
    }

    void RenderTarget::updateClipping(const tgui::FloatRect clipRect, const tgui::FloatRect) {
        validate_rectangle(clipRect);
        if (!drawable() || clipRect.width == 0 || clipRect.height == 0) {
            clip_rectangle_ = {};
            return;
        }
        if (clipRect == m_viewRect) {
            clip_rectangle_ = viewport_rectangle(m_viewport);
            return;
        }
        // TGUI intersects in view space, then builds a float viewport whose
        // accumulated error can turn an exact edge into an extra scissor pixel.
        // Map each original edge once, retaining double precision until playback.
        const auto horizontal = [&](const double edge) {
            return m_viewport.left + ((edge - m_viewRect.left) * m_viewport.width / m_viewRect.width);
        };
        const auto vertical = [&](const double edge) {
            return m_viewport.top + ((edge - m_viewRect.top) * m_viewport.height / m_viewRect.height);
        };
        clip_rectangle_ = {horizontal(clipRect.left), vertical(clipRect.top),
                           horizontal(static_cast<double>(clipRect.left) + clipRect.width),
                           vertical(static_cast<double>(clipRect.top) + clipRect.height)};
    }

    void RenderTarget::drawVertexArray(
        const tgui::RenderStates& states,
        const tgui::Vertex* vertices,
        const std::size_t vertexCount,
        const unsigned int* indices,
        const std::size_t indexCount,
        const std::shared_ptr<tgui::BackendTexture>& texture
    ) {
        require_recording();
        if (!indices && indexCount != 0)
            throw Exceptions::invalid_args(CE_HERE, "TGUI index counts require index storage");
        const auto count = indices ? indexCount : vertexCount;
        if (count == 0)
            return;
        if (!vertices || vertexCount == 0 || count % 3 != 0)
            throw Exceptions::invalid_args(CE_HERE, "TGUI recording needs complete triangles");
        validate_transform(states.transform);
        RecordedDraw draw;
        if (texture) {
            const auto owned = std::dynamic_pointer_cast<Texture>(texture);
            if (!owned || !owned->snapshot())
                throw Exceptions::invalid_args(CE_HERE, "TGUI recording needs a loaded Cheryl texture");
            draw.texture = owned->snapshot();
        }
        if (!drawable() || clip_rectangle_.left == clip_rectangle_.right || clip_rectangle_.top == clip_rectangle_.bottom)
            return;
        draw.clip = clip_region(clip_rectangle_, m_targetSize);
        draw.vertices.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            const auto index = indices ? indices[i] : i;
            if (index >= vertexCount)
                throw Exceptions::invalid_args(CE_HERE, "TGUI triangle index exceeds its vertex storage");
            const auto& source = vertices[index];
            const auto position = states.transform.transformPoint(source.position);
            const float x = m_viewport.left + ((position.x - m_viewRect.left) / m_viewRect.width) * m_viewport.width;
            const float y = m_viewport.top + ((position.y - m_viewRect.top) / m_viewRect.height) * m_viewport.height;
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(source.texCoords.x) || !std::isfinite(source.texCoords.y))
                throw Exceptions::invalid_args(CE_HERE, "TGUI vertex coordinates must be finite and representable");
            draw.vertices.push_back(
                {x, y, 0, source.texCoords.x, draw.texture ? 1.0f - source.texCoords.y : source.texCoords.y, source.color.red / 255.0f,
                 source.color.green / 255.0f, source.color.blue / 255.0f, source.color.alpha / 255.0f}
            );
        }
        draws_.push_back(std::move(draw));
    }

    void RenderTarget::setClearColor(const tgui::Color&) {
        throw Exceptions::failed_operation(CE_HERE, "The application owns Cheryl platform clear state");
    }

    void RenderTarget::clearScreen() {
        throw Exceptions::failed_operation(CE_HERE, "The application clears the platform framebuffer outside TGUI recording");
    }
}
