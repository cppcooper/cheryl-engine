#include <backends/opengl/gl.h>
#include <backends/opengl/renderer.h>

#include <assets/types/primitives/vertex.h>
#include <internals/exceptions.h>

#include <ext/matrix_transform.hpp>

#include <thread>
#include <type_traits>

namespace CE::RenderAPIs {
    namespace {
        void draw_grid_cell(const Assets::Asset2D& asset, const Assets::GridDefinition& grid, const Assets::CellIndex cell) {
            if (cell >= grid.cell_count())
                throw Exceptions::invalid_args(CE_HERE, "Render command selects a cell outside its grid");
            if (!asset.geometry || !asset.texture)
                throw Exceptions::invalid_args(CE_HERE, "Render command has incomplete grid resources");
            asset.geometry->bind(*asset.texture);
            asset.geometry->draw(cell * VAONumbers::vertices_per_strip_quad, VAONumbers::vertices_per_strip_quad);
        }
    }

    OpenGLRenderer::OpenGLRenderer(iOpenGLContext& context)
    : context_(context) {}

    OpenGLRenderer::~OpenGLRenderer() {
        if (initialized_) {
            try {
                deinitialize();
            }
            catch (...) {
                // Retained assets must never query a context after its owner is destroyed.
                resources_->abandon();
            }
        }
    }

    void OpenGLRenderer::initialize() {
        if (initialized_) {
            resources_->require_owner();
            context_.make_current();
            resources_->require_current();
            return;
        }
        if (stopped_)
            throw Exceptions::failed_operation(CE_HERE, "Create a new renderer after OpenGL shutdown");
        context_.make_current();
        try {
            if (!context_.is_current())
                throw Exceptions::failed_operation(CE_HERE, "OpenGL initialization requires its current context");
            // The renderer receives procedure addresses through the context interface,
            // without depending on the display's native window type.
            const auto version = gladLoadGLUserPtr(
                [](void* user, const char* name) -> GLADapiproc { return static_cast<iOpenGLContext*>(user)->proc_address(name); },
                &context_);
            if (version == 0 || GLAD_VERSION_MAJOR(version) < 3 || (GLAD_VERSION_MAJOR(version) == 3 && GLAD_VERSION_MINOR(version) < 3)) {
                throw Exceptions::failed_operation(CE_HERE, "An OpenGL 3.3 context is required");
            }
            resources_ = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(),
                                                                  [&context = context_] { return context.is_current(); });
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        }
        catch (...) {
            // Preserve the initialization failure even if releasing the context also fails.
            try {
                context_.release_current();
            }
            catch (...) {}
            throw;
        }
        initialized_ = true;
    }

    void OpenGLRenderer::deinitialize() {
        if (!initialized_)
            return;
        resources_->require_owner();
        context_.make_current();
        resources_->shutdown();
        initialized_ = false;
        stopped_ = true;
        context_.release_current();
    }

    std::shared_ptr<OpenGLResourceLifetime> OpenGLRenderer::resources() const {
        if (!initialized_)
            throw Exceptions::failed_operation(CE_HERE, "OpenGL renderer must be initialized before uploading resources");
        resources_->require_current();
        return resources_;
    }

    void OpenGLRenderer::bind_style(const DrawStyle& style, Assets::Shader*& active_material) const {
        if (!style.material)
            throw Exceptions::invalid_args(CE_HERE, "Render command needs a material");
        auto* material = style.material.get();
        if (material != active_material) {
            material->bind_pass({projection_, view_});
            active_material = material;
        }
        material->bind_draw({style.model_matrix, style.alpha, 1.0f, 0});
    }

    void OpenGLRenderer::render(const RenderFrame& frame) {
        resources()->collect();
        for (const auto& pass : frame.passes()) {
            set_depth_test(pass.depth_test);
            set_camera_matrices(pass.projection, pass.view);
            Assets::Shader* active_material = nullptr;
            for (const auto& command : pass.draws) {
                std::visit(
                    [&](const auto& draw) {
                        using Draw = std::decay_t<decltype(draw)>;
                        if constexpr (std::is_same_v<Draw, SpriteDraw>) {
                            if (!draw.sprite)
                                throw Exceptions::invalid_args(CE_HERE, "Sprite draw has no sprite");
                            bind_style(draw.style, active_material);
                            draw_grid_cell(*draw.sprite, draw.sprite->definition().grid, draw.cell);
                        }
                        else if constexpr (std::is_same_v<Draw, TileDraw>) {
                            if (!draw.tileset)
                                throw Exceptions::invalid_args(CE_HERE, "Tile draw has no tileset");
                            bind_style(draw.style, active_material);
                            draw_grid_cell(*draw.tileset, draw.tileset->definition().grid, draw.cell);
                        }
                        else if constexpr (std::is_same_v<Draw, GraphicDraw>) {
                            if (!draw.graphic || !draw.graphic->geometry || !draw.graphic->texture)
                                throw Exceptions::invalid_args(CE_HERE, "Graphic draw has incomplete resources");
                            bind_style(draw.style, active_material);
                            draw.graphic->geometry->bind(*draw.graphic->texture);
                            draw.graphic->geometry->draw(0, VAONumbers::vertices_per_quad);
                        }
                        else if constexpr (std::is_same_v<Draw, TextDraw>) {
                            if (!draw.font)
                                throw Exceptions::invalid_args(CE_HERE, "Text draw has no font");
                            bind_style(draw.style, active_material);
                            const auto& geometry = draw.font->glyph_geometry();
                            geometry.bind(draw.font->glyph_atlas());
                            // The font and message are read-only. Only the model uniform
                            // changes as the pen advances through pre-uploaded glyphs.
                            draw.font->for_each_glyph(draw.text, [&](const std::size_t index, const float x, const float y) {
                                const auto model = glm::translate(draw.style.model_matrix, glm::vec3(x, y, 0.0f));
                                active_material->bind_draw({model, draw.style.alpha, 1.0f, 0});
                                geometry.draw(index * VAONumbers::vertices_per_quad, VAONumbers::vertices_per_quad);
                            });
                        }
                    },
                    command);
            }
        }
    }

    void OpenGLRenderer::clear() {
        resources()->collect();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::set_viewport(const FramebufferSize size) {
        (void)resources();
        if (size.width < 0 || size.height < 0)
            throw Exceptions::invalid_args(CE_HERE, "Framebuffer dimensions cannot be negative");
        glViewport(0, 0, size.width, size.height);
    }

    void OpenGLRenderer::set_depth_test(const bool enabled) {
        (void)resources();
        if (enabled)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
    }

    void OpenGLRenderer::set_clear_colour(const float r, const float g, const float b, const float a) {
        (void)resources();
        glClearColor(r, g, b, a);
    }

    void OpenGLRenderer::set_camera_matrices(const glm::mat4& projection, const glm::mat4& view) {
        (void)resources();
        projection_ = projection;
        view_ = view;
    }
}
