#include <backends/opengl/pipeline.h>
#include <backends/opengl/texture.h>
#include <backends/opengl/sampler.h>
#include <backends/opengl/vertex-array-object.h>
#include <backends/opengl/resource-lifetime-internal.h>
#include <backends/opengl/renderer-internal.h>
#include <backends/opengl/resource-provider.h>
#include <core/rendering/render-frame.h>
#include <core/resources/asset-management/material-mgr.h>
#include <testing/failing-memory-resource.h>

#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <array>
#include <functional>
#include <future>
#include <map>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {
    using namespace CE::Assets;
    using namespace CE::RenderAPIs;
    using CE::Exceptions::invalid_args;

    // Synthetic IDs and recording GLAD entries exercise contract code without a window.
    // All entries are restored, including when fixture initialization throws.
    class NativeProgramRecorder final {
        inline static NativeProgramRecorder* active_ = nullptr;
        std::vector<std::function<void()>> restore_;
        bool current_ = true;
        int current_queries_ = 0;
        std::shared_ptr<OpenGLResourceLifetime> lifetime_ = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [this] {
            ++current_queries_;
            return current_;
        });
        std::vector<std::shared_ptr<OpenGLResourceLifetime>> other_domains_;
        GLuint next_id_ = 10;
        std::uint32_t active_unit_ = 0;
        GLenum error_ = GL_NO_ERROR;
        std::shared_ptr<CE::Testing::FailingMemoryResource> entry_memory_;
        std::vector<OpenGLHandle> registration_padding_;
        int reject_registration_at_ = 0;
        int registration_candidates_ = 0;

    public:
        bool linked = true;
        std::vector<GLSLVariable> uniforms;
        std::vector<GLSLVariable> attributes{{"in_Position", GL_FLOAT_VEC3, 1, 0}, {"in_Texcoord", GL_FLOAT_VEC2, 1, 1}};
        std::map<GLint, ParameterValue> writes;
        std::vector<std::pair<std::uint32_t, GLuint>> image_binds;
        std::vector<std::pair<std::uint32_t, GLuint>> sampler_binds;
        std::map<GLuint, std::map<GLenum, GLint>> sampler_parameters;
        std::map<GLuint, GLfloat> sampler_anisotropy;
        std::map<GLenum, GLint> image_parameters;
        GLfloat image_anisotropy = 1;
        int uses = 0;
        std::map<GLenum, bool> enabled;
        std::array<GLenum, 4> blend_factors{};
        std::pair<GLenum, GLenum> blend_equations{};
        GLenum depth_function = GL_LESS;
        GLboolean depth_write = GL_TRUE;
        GLenum cull_face = GL_BACK;
        GLenum front_face = GL_CCW;
        int state_changes = 0;
        int geometry_binds = 0;
        std::vector<std::pair<GLint, GLsizei>> draws;
        std::vector<std::pair<GLResourceKind, GLuint>> generated;
        std::vector<std::pair<GLResourceKind, GLuint>> deleted;
        int buffer_uploads = 0;
        int fail_buffer_upload = 0;
        bool fail_image_upload = false;
        bool fail_anisotropy_query = false;
        GLfloat maximum_anisotropy = 16;
        int anisotropy_queries = 0;
        bool fail_sampler_parameters = false;
        std::optional<GLenum> fail_integer_query;
        int anisotropy_writes = 0;
        int image_uploads = 0;
        bool fail_mipmaps = false;
        int mipmap_calls = 0;
        int attributes_enabled = 0;
        std::map<GLuint, std::pair<GLint, GLsizei>> attribute_layouts;
        std::array<GLint, 4> scissor_box{};
        GLint unpack_alignment = 4;
        std::optional<GLResourceKind> fail_generation;
        bool lose_context_on_error = false;
        std::string fail_startup_operation;

    private:
        template <typename T> void replace(T& entry, T replacement) {
            const auto old = entry;
            restore_.push_back([&entry, old] { entry = old; });
            entry = replacement;
        }

        void restore() noexcept {
            for (auto item = restore_.rbegin(); item != restore_.rend(); ++item)
                (*item)();
            restore_.clear();
            active_ = nullptr;
        }

        void prepare_registration_failure() {
            if (++registration_candidates_ != reject_registration_at_)
                return;
            // Fill only spare capacity so the next native adoption must request
            // registry storage, independent of the vector's growth strategy.
            while (ResourceDetail::LifetimeAccess::size(*lifetime_) < ResourceDetail::LifetimeAccess::capacity(*lifetime_)) {
                const auto id = ++next_id_;
                generated.emplace_back(GLResourceKind::Buffer, id);
                registration_padding_.emplace_back(lifetime_, GLResourceKind::Buffer, id);
            }
            entry_memory_->reject_next();
        }

        static GLint maximum_name_length(const std::vector<GLSLVariable>& variables) {
            std::size_t longest = 0;
            for (const auto& variable : variables)
                longest = std::max(longest, variable.name.size());
            return static_cast<GLint>(longest + 1);
        }

        static void GLAD_API_PTR program_query(GLuint, GLenum parameter, GLint* value) {
            switch (parameter) {
                case GL_LINK_STATUS:
                    *value = active_->linked ? GL_TRUE : GL_FALSE;
                    break;
                case GL_ACTIVE_UNIFORMS:
                    *value = static_cast<GLint>(active_->uniforms.size());
                    break;
                case GL_ACTIVE_UNIFORM_MAX_LENGTH:
                    *value = maximum_name_length(active_->uniforms);
                    break;
                case GL_ACTIVE_ATTRIBUTES:
                    *value = static_cast<GLint>(active_->attributes.size());
                    break;
                case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
                    *value = maximum_name_length(active_->attributes);
                    break;
                default:
                    *value = 0;
                    break;
            }
        }

        static void
        variable_query(const GLSLVariable& variable, GLsizei capacity, GLsizei* written, GLint* size, GLenum* type, GLchar* name) {
            const auto count = std::min(variable.name.size(), static_cast<std::size_t>(std::max(0, capacity - 1)));
            std::copy_n(variable.name.data(), count, name);
            name[count] = '\0';
            *written = static_cast<GLsizei>(count);
            *size = variable.size;
            *type = variable.type;
        }

        static void GLAD_API_PTR
        uniform_query(GLuint, GLuint index, GLsizei capacity, GLsizei* written, GLint* size, GLenum* type, GLchar* name) {
            variable_query(active_->uniforms.at(index), capacity, written, size, type, name);
        }

        static void GLAD_API_PTR
        attribute_query(GLuint, GLuint index, GLsizei capacity, GLsizei* written, GLint* size, GLenum* type, GLchar* name) {
            variable_query(active_->attributes.at(index), capacity, written, size, type, name);
        }

        static GLint location(const std::vector<GLSLVariable>& variables, const GLchar* name) {
            const auto variable = std::find_if(variables.begin(), variables.end(), [&](const auto& item) { return item.name == name; });
            return variable == variables.end() ? -1 : variable->location;
        }
        static GLint GLAD_API_PTR uniform_location(GLuint, const GLchar* name) { return location(active_->uniforms, name); }
        static GLint GLAD_API_PTR attribute_location(GLuint, const GLchar* name) { return location(active_->attributes, name); }
        static void GLAD_API_PTR use(GLuint) { ++active_->uses; }
        static void GLAD_API_PTR scalar(GLint location, GLfloat value) { active_->writes.insert_or_assign(location, value); }
        static void GLAD_API_PTR integer(GLint location, GLint value) { active_->writes.insert_or_assign(location, value); }
        static void GLAD_API_PTR colour(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
            active_->writes.insert_or_assign(location, glm::vec4{x, y, z, w});
        }
        static void GLAD_API_PTR matrix(GLint location, GLsizei, GLboolean, const GLfloat* values) {
            glm::mat4 copied{0.0f};
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    copied[column][row] = values[column * 4 + row];
            active_->writes.insert_or_assign(location, copied);
        }
        static void generate(GLResourceKind kind, GLsizei count, GLuint* images) {
            for (GLsizei i = 0; i < count; ++i)
                active_->generated.emplace_back(kind, images[i] = ++active_->next_id_);
            if (active_->fail_generation == kind)
                active_->error_ = GL_OUT_OF_MEMORY;
            active_->prepare_registration_failure();
        }
        static void GLAD_API_PTR generate_images(GLsizei count, GLuint* images) { generate(GLResourceKind::Texture, count, images); }
        static void GLAD_API_PTR generate_buffers(GLsizei count, GLuint* images) { generate(GLResourceKind::Buffer, count, images); }
        static void GLAD_API_PTR generate_arrays(GLsizei count, GLuint* images) { generate(GLResourceKind::VertexArray, count, images); }
        static void GLAD_API_PTR generate_samplers(GLsizei count, GLuint* ids) { generate(GLResourceKind::Sampler, count, ids); }
        static void delete_ids(GLResourceKind kind, GLsizei count, const GLuint* ids) {
            for (GLsizei i = 0; i < count; ++i)
                active_->deleted.emplace_back(kind, ids[i]);
        }
        static void GLAD_API_PTR delete_images(GLsizei count, const GLuint* ids) { delete_ids(GLResourceKind::Texture, count, ids); }
        static void GLAD_API_PTR delete_buffers(GLsizei count, const GLuint* ids) { delete_ids(GLResourceKind::Buffer, count, ids); }
        static void GLAD_API_PTR delete_arrays(GLsizei count, const GLuint* ids) { delete_ids(GLResourceKind::VertexArray, count, ids); }
        static void GLAD_API_PTR delete_program(GLuint id) { delete_ids(GLResourceKind::Program, 1, &id); }
        static void GLAD_API_PTR delete_samplers(GLsizei count, const GLuint* ids) { delete_ids(GLResourceKind::Sampler, count, ids); }
        static GLenum GLAD_API_PTR error_query() {
            const auto error = std::exchange(active_->error_, GL_NO_ERROR);
            if (error != GL_NO_ERROR && active_->lose_context_on_error)
                active_->current_ = false;
            return error;
        }
        static void GLAD_API_PTR activate_image(GLenum unit) { active_->active_unit_ = unit - GL_TEXTURE0; }
        static void GLAD_API_PTR bind_image(GLenum, GLuint image) { active_->image_binds.emplace_back(active_->active_unit_, image); }
        static void GLAD_API_PTR bind_sampler(GLuint unit, GLuint sampler) { active_->sampler_binds.emplace_back(unit, sampler); }
        static void GLAD_API_PTR sampler_parameter(GLuint sampler, GLenum parameter, GLint value) {
            active_->sampler_parameters[sampler][parameter] = value;
            if (active_->fail_sampler_parameters)
                active_->error_ = GL_OUT_OF_MEMORY;
        }
        static void GLAD_API_PTR sampler_anisotropy_parameter(GLuint sampler, GLenum, GLfloat value) {
            active_->sampler_anisotropy[sampler] = value;
        }
        static void GLAD_API_PTR integer_query(GLenum parameter, GLint* value) {
            if (active_->fail_integer_query == parameter) {
                active_->error_ = GL_INVALID_OPERATION;
                return; // Failed queries deliberately leave the caller's output untouched.
            }
            *value = parameter == GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS ? 8 : active_->unpack_alignment;
        }
        static void GLAD_API_PTR anisotropy_query(GLenum, GLfloat* value) {
            ++active_->anisotropy_queries;
            if (active_->fail_anisotropy_query) {
                active_->error_ = GL_INVALID_ENUM;
                return;
            }
            *value = active_->maximum_anisotropy;
        }
        static void GLAD_API_PTR anisotropy_parameter(GLenum, GLenum, GLfloat value) {
            ++active_->anisotropy_writes;
            active_->image_anisotropy = value;
        }
        static void GLAD_API_PTR image_parameter(GLenum, GLenum parameter, GLint value) { active_->image_parameters[parameter] = value; }
        static void GLAD_API_PTR image_parameters(GLenum, GLenum, const GLint*) {}
        static void GLAD_API_PTR pixel_store(GLenum parameter, GLint value) {
            if (parameter == GL_UNPACK_ALIGNMENT)
                active_->unpack_alignment = value;
        }
        static void GLAD_API_PTR upload_image(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) {
            ++active_->image_uploads;
            if (active_->fail_image_upload)
                active_->error_ = GL_OUT_OF_MEMORY;
        }
        static void GLAD_API_PTR mipmaps(GLenum) {
            ++active_->mipmap_calls;
            if (active_->fail_mipmaps)
                active_->error_ = GL_OUT_OF_MEMORY;
        }
        static void GLAD_API_PTR enable(GLenum capability) {
            active_->enabled[capability] = true;
            ++active_->state_changes;
            if (active_->fail_startup_operation == "enable")
                active_->error_ = GL_INVALID_OPERATION;
        }
        static void GLAD_API_PTR startup_blend(GLenum, GLenum) {
            ++active_->state_changes;
            if (active_->fail_startup_operation == "blend")
                active_->error_ = GL_INVALID_OPERATION;
        }
        static void GLAD_API_PTR clear_colour(GLfloat, GLfloat, GLfloat, GLfloat) {
            ++active_->state_changes;
            if (active_->fail_startup_operation == "clear")
                active_->error_ = GL_INVALID_OPERATION;
        }
        static void GLAD_API_PTR disable(GLenum capability) {
            active_->enabled[capability] = false;
            ++active_->state_changes;
        }
        static void GLAD_API_PTR blend_equation(GLenum rgb, GLenum alpha) {
            active_->blend_equations = {rgb, alpha};
            ++active_->state_changes;
        }
        static void GLAD_API_PTR blend_function(GLenum source, GLenum target, GLenum alpha_source, GLenum alpha_target) {
            active_->blend_factors = {source, target, alpha_source, alpha_target};
            ++active_->state_changes;
        }
        static void GLAD_API_PTR set_depth_function(GLenum function) {
            active_->depth_function = function;
            ++active_->state_changes;
        }
        static void GLAD_API_PTR depth_mask(GLboolean enabled) {
            active_->depth_write = enabled;
            ++active_->state_changes;
        }
        static void GLAD_API_PTR cull(GLenum face) {
            active_->cull_face = face;
            ++active_->state_changes;
        }
        static void GLAD_API_PTR winding(GLenum face) {
            active_->front_face = face;
            ++active_->state_changes;
        }
        static void GLAD_API_PTR bind_geometry(GLuint) { ++active_->geometry_binds; }
        static void GLAD_API_PTR bind_buffer(GLenum, GLuint) {}
        static void GLAD_API_PTR upload_buffer(GLenum, GLsizeiptr, const void*, GLenum) {
            if (++active_->buffer_uploads == active_->fail_buffer_upload)
                active_->error_ = GL_OUT_OF_MEMORY;
        }
        static void GLAD_API_PTR enable_attribute(GLuint) { ++active_->attributes_enabled; }
        static void GLAD_API_PTR attribute_pointer(GLuint index, GLint count, GLenum, GLboolean, GLsizei stride, const void*) {
            active_->attribute_layouts[index] = {count, stride};
        }
        static void GLAD_API_PTR scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
            active_->scissor_box = {x, y, width, height};
            ++active_->state_changes;
        }
        static void GLAD_API_PTR draw_geometry(GLenum, GLint first, GLsizei count) { active_->draws.emplace_back(first, count); }

    public:
        NativeProgramRecorder() {
            active_ = this;
            try {
                replace(glad_glGetProgramiv, program_query);
                replace(glad_glGetActiveUniform, uniform_query);
                replace(glad_glGetActiveAttrib, attribute_query);
                replace(glad_glGetUniformLocation, uniform_location);
                replace(glad_glGetAttribLocation, attribute_location);
                replace(glad_glUseProgram, use);
                replace(glad_glUniform1f, scalar);
                replace(glad_glUniform1i, integer);
                replace(glad_glUniform4f, colour);
                replace(glad_glUniformMatrix4fv, matrix);
                replace(glad_glGenTextures, generate_images);
                replace(glad_glActiveTexture, activate_image);
                replace(glad_glBindTexture, bind_image);
                replace(glad_glGenSamplers, generate_samplers);
                replace(glad_glDeleteSamplers, delete_samplers);
                replace(glad_glBindSampler, bind_sampler);
                replace(glad_glSamplerParameteri, sampler_parameter);
                replace(glad_glSamplerParameterf, sampler_anisotropy_parameter);
                replace(glad_glGetIntegerv, integer_query);
                replace(glad_glGetFloatv, anisotropy_query);
                replace(glad_glGetError, error_query);
                replace(glad_glTexParameteri, image_parameter);
                replace(glad_glTexParameterf, anisotropy_parameter);
                replace(glad_glTexParameteriv, image_parameters);
                replace(glad_glPixelStorei, pixel_store);
                replace(glad_glTexImage2D, upload_image);
                replace(glad_glGenerateMipmap, mipmaps);
                replace(glad_glDeleteTextures, delete_images);
                replace(glad_glDeleteBuffers, delete_buffers);
                replace(glad_glDeleteVertexArrays, delete_arrays);
                replace(glad_glDeleteProgram, delete_program);
                replace(GLAD_GL_EXT_texture_filter_anisotropic, 0);
                replace(GLAD_GL_ARB_texture_filter_anisotropic, 0);
                replace(GLAD_GL_VERSION_4_6, 0);
                replace(glad_glEnable, enable);
                replace(glad_glBlendFunc, startup_blend);
                replace(glad_glClearColor, clear_colour);
                replace(glad_glDisable, disable);
                replace(glad_glScissor, scissor);
                replace(glad_glBlendEquationSeparate, blend_equation);
                replace(glad_glBlendFuncSeparate, blend_function);
                replace(glad_glDepthFunc, set_depth_function);
                replace(glad_glDepthMask, depth_mask);
                replace(glad_glCullFace, cull);
                replace(glad_glFrontFace, winding);
                replace(glad_glGenVertexArrays, generate_arrays);
                replace(glad_glGenBuffers, generate_buffers);
                replace(glad_glBindVertexArray, bind_geometry);
                replace(glad_glBindBuffer, bind_buffer);
                replace(glad_glBufferData, upload_buffer);
                replace(glad_glEnableVertexAttribArray, enable_attribute);
                replace(glad_glVertexAttribPointer, attribute_pointer);
                replace(glad_glDrawArrays, draw_geometry);
            } catch (...) {
                restore();
                throw;
            }
        }
        ~NativeProgramRecorder() {
            lifetime_->abandon();
            for (const auto& domain : other_domains_)
                domain->abandon();
            restore();
        }
        NativeProgramRecorder(const NativeProgramRecorder&) = delete;
        NativeProgramRecorder& operator=(const NativeProgramRecorder&) = delete;

        void pending_error(GLenum error) { error_ = error; }
        void enable_anisotropy(const int source = 0) {
            if (source == 0)
                replace(GLAD_GL_EXT_texture_filter_anisotropic, 1);
            else if (source == 1)
                replace(GLAD_GL_ARB_texture_filter_anisotropic, 1);
            else
                replace(GLAD_GL_VERSION_4_6, 1);
        }
        void reject_registration(const int candidate) {
            entry_memory_ = std::make_shared<CE::Testing::FailingMemoryResource>();
            lifetime_ = ResourceDetail::LifetimeAccess::create(
                std::this_thread::get_id(),
                [this] {
                    ++current_queries_;
                    return current_;
                },
                entry_memory_
            );
            reject_registration_at_ = candidate;
        }
        void release_registration_padding() { registration_padding_.clear(); }
        [[nodiscard]] std::size_t rejected_allocations() const { return entry_memory_->rejected.load(); }
        void collect() { lifetime_->collect(); }
        void set_current(const bool current) { current_ = current; }
        [[nodiscard]] bool is_current() const { return current_; }
        [[nodiscard]] int current_queries() const { return current_queries_; }
        std::shared_ptr<OpenGLResourceLifetime> lifetime() { return lifetime_; }

        std::shared_ptr<GLSLProgram> program() {
            return std::make_shared<GLSLProgram>(OpenGLHandle(lifetime_, GLResourceKind::Program, ++next_id_));
        }
        std::shared_ptr<Texture> image(bool another_domain = false) {
            auto domain = lifetime_;
            if (another_domain) {
                domain = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [] { return true; });
                other_domains_.push_back(domain);
            }
            const unsigned char pixels[]{255, 255, 255, 255};
            return std::make_shared<Texture>(domain, pixels, 1, 1, false, false, GL_CLAMP_TO_EDGE, GL_RGBA);
        }
        std::shared_ptr<OpenGLSampler> sampler(const SamplerOptions& options, const bool another_domain = false) {
            auto domain = lifetime_;
            if (another_domain) {
                domain = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [] { return true; });
                other_domains_.push_back(domain);
            }
            return std::make_shared<OpenGLSampler>(domain, options);
        }
        std::shared_ptr<CE::VAO> geometry(bool another_domain = false) {
            auto domain = lifetime_;
            if (another_domain) {
                domain = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [] { return true; });
                other_domains_.push_back(domain);
            }
            const std::array<CE::Vertex2D, 6> vertices{};
            return std::make_shared<CE::VAO>(domain, std::span<const CE::Vertex2D>{vertices}, PrimitiveTopology::Triangles);
        }
    };

    class RecordingContext final : public iOpenGLContext {
        NativeProgramRecorder& native_;

    public:
        bool fail_acquisition = false;
        bool fail_release = false;
        int acquisitions = 0;
        int releases = 0;
        mutable int queries = 0;

        explicit RecordingContext(NativeProgramRecorder& native)
        : native_(native) {}

        void make_current() override {
            ++acquisitions;
            if (fail_acquisition) {
                native_.set_current(false);
                throw CE::Exceptions::failed_operation(CE_HERE, "Controlled context recovery failure");
            }
            native_.set_current(true);
        }
        void release_current() override {
            ++releases;
            native_.set_current(false);
            if (fail_release)
                throw std::runtime_error("Later context release failure");
        }
        [[nodiscard]] bool is_current() const override {
            ++queries;
            return native_.is_current();
        }
        [[nodiscard]] ProcAddress proc_address(const char*) const override {
            throw std::logic_error("Recording entries are already installed");
        }
        void present() override {}
    };

    PipelineDefinition time_definition(bool required = true) {
        PipelineDefinition definition;
        definition.program_sources = {"effect.vert", "effect.frag"};
        definition.parameters = {{"time", ParameterType::Float, required}};
        return definition;
    }
}

TEST(opengl_renderer, startup_failure) {
    for (const std::string operation : {"pending", "loader", "enable", "blend", "clear"}) {
        for (const bool release_failure : {false, true}) {
            SCOPED_TRACE(operation);
            SCOPED_TRACE(release_failure);
            NativeProgramRecorder native;
            RecordingContext context(native);
            context.fail_release = release_failure;
            OpenGLRenderer renderer(context);
            RendererDetail::RendererAccess::set_native_loader(renderer, [&](iOpenGLContext&) {
                if (operation == "loader")
                    throw std::runtime_error("Original loader failure");
            });
            native.fail_startup_operation = operation;
            if (operation == "pending")
                native.pending_error(GL_INVALID_ENUM);
            if (operation == "loader") {
                try {
                    renderer.initialize();
                    FAIL() << "Loader must fail";
                } catch (const std::runtime_error& error) {
                    EXPECT_EQ(std::string_view(error.what()), "Original loader failure");
                }
            } else {
                try {
                    renderer.initialize();
                    FAIL() << "Native startup must fail";
                } catch (const CE::Exceptions::failed_operation& error) {
                    const auto expected = operation == "pending" ? "before renderer startup" : "startup configuration failed";
                    EXPECT_NE(std::string(error.what()).find(expected), std::string::npos);
                }
            }
            EXPECT_EQ(context.releases, 1);
            EXPECT_FALSE(context.is_current());
            EXPECT_THROW((void)renderer.resources(), CE::Exceptions::failed_operation);
            EXPECT_NO_THROW(renderer.deinitialize());
            EXPECT_EQ(context.releases, 1);
            EXPECT_TRUE(native.generated.empty());
            if (operation == "pending" || operation == "loader")
                EXPECT_EQ(native.state_changes, 0);
        }
    }
}

TEST(opengl_renderer, context_loss_ownership) {
    for (const bool recover : {false, true}) {
        for (const bool release_failure : {false, true}) {
            SCOPED_TRACE(recover ? "recover" : "abandon");
            SCOPED_TRACE(release_failure);
            NativeProgramRecorder native;
            RecordingContext context(native);
            auto renderer = std::make_unique<OpenGLRenderer>(context);
            RendererDetail::RendererAccess::set_native_loader(*renderer, [](iOpenGLContext&) {});
            renderer->initialize();
            auto domain = renderer->resources();
            auto provider = std::make_unique<OpenGLResourceProvider>(*renderer);
            auto& cache = MaterialMgr::get();
            const std::filesystem::path key{"context-loss-material"};
            native.uniforms = {{"uImage", GL_SAMPLER_2D, 1, 8}};
            native.generated.emplace_back(GLResourceKind::Program, GLuint{500});
            auto program = std::make_shared<GLSLProgram>(OpenGLHandle(domain, GLResourceKind::Program, 500));
            PipelineDefinition definition;
            definition.program_sources = {"retained.vert", "retained.frag"};
            definition.parameters = {{"image", ParameterType::Sampler2D, true}};
            auto pipeline = std::make_shared<GLSLPipeline>(definition, program, GLSLPipelineBindings{{{"image", "uImage"}}});
            DecodedImage pixels{PixelSize{1, 1}, {255, 255, 255, 255}};
            auto image = provider->create_image(pixels);
            const std::array<CE::Vertex2D, 6> vertices{};
            auto geometry = provider->upload_geometry(vertices, PrimitiveTopology::Triangles);
            auto material = provider->build_material({pipeline, {{"image", ImageBinding{image, 0}}}});
            cache.load_material(key, *provider, [&](ResourceProvider&) { return material; });
            auto retained = cache.get_asset(key);
            std::weak_ptr<GLSLProgram> program_owner = program;
            std::weak_ptr<Image> image_owner = image;
            std::weak_ptr<Geometry2D> geometry_owner = geometry;
            RenderFrame frame;
            {
                RenderFrameWriter writer(frame);
                auto pass = writer.begin_pass(glm::mat4{1.0f}, glm::mat4{1.0f});
                DrawStyle2D style;
                style.material = material;
                pass.add(resolve_draw_packet(geometry, 0, 6, style, pass.semantics(), pass.parameters(), pass.constraints()));
            }
            program.reset();
            pipeline.reset();
            image.reset();
            geometry.reset();
            material.reset();
            native.set_current(false);
            const auto before_loss = native.state_changes;
            EXPECT_THROW(renderer->render(frame), CE::Exceptions::failed_operation);
            EXPECT_EQ(native.state_changes, before_loss);
            EXPECT_TRUE(native.draws.empty());
            EXPECT_THROW(
                cache.reload_material(
                    key, *provider,
                    [&](ResourceProvider& value) {
                        return static_cast<OpenGLResourceProvider&>(value).build_material(retained->definition());
                    }
                ),
                CE::Exceptions::failed_operation
            );
            EXPECT_EQ(cache.get_asset(key), retained);
            cache.clear_assets();
            provider.reset();
            EXPECT_FALSE(program_owner.expired());
            EXPECT_FALSE(image_owner.expired());
            EXPECT_FALSE(geometry_owner.expired());
            EXPECT_TRUE(native.deleted.empty());
            context.fail_release = release_failure;
            if (recover) {
                if (release_failure)
                    EXPECT_THROW(renderer->deinitialize(), std::runtime_error);
                else
                    EXPECT_NO_THROW(renderer->deinitialize());
                auto expected = native.generated;
                auto deleted = native.deleted;
                std::sort(expected.begin(), expected.end());
                std::sort(deleted.begin(), deleted.end());
                EXPECT_EQ(deleted, expected);
            } else {
                context.fail_acquisition = true;
                renderer.reset(); // Destructor recovery fails, then abandons the retained domain.
                EXPECT_TRUE(native.deleted.empty());
            }
            const auto after_closure = context.queries;
            EXPECT_THROW(domain->require_current(), CE::Exceptions::failed_operation);
            std::thread release([&frame, retained = std::move(retained)]() mutable {
                frame.recycle();
                retained.reset();
            });
            release.join();
            EXPECT_TRUE(program_owner.expired());
            EXPECT_TRUE(image_owner.expired());
            EXPECT_TRUE(geometry_owner.expired());
            EXPECT_EQ(context.queries, after_closure);
            if (recover) {
                EXPECT_EQ(native.deleted.size(), native.generated.size());
                EXPECT_THROW((void)renderer->resources(), CE::Exceptions::failed_operation);
                renderer.reset();
            } else
                EXPECT_TRUE(native.deleted.empty());
        }
    }
}

TEST(opengl_lifetime, generation_context_loss) {
    NativeProgramRecorder native;
    auto retained = native.image();
    native.fail_generation = GLResourceKind::Buffer;
    native.lose_context_on_error = true;
    auto vertices = std::shared_ptr<CE::Vertex2D>(new CE::Vertex2D[6]{}, std::default_delete<CE::Vertex2D[]>{});
    std::weak_ptr<CE::Vertex2D> cpu_owner = vertices;
    try {
        static_cast<void>(std::make_shared<CE::VAO>(native.lifetime(), std::move(vertices), 6, PrimitiveTopology::Triangles));
        FAIL() << "Buffer generation must fail";
    } catch (const CE::Exceptions::failed_operation& error) {
        EXPECT_NE(std::string(error.what()).find("buffer creation failed"), std::string::npos);
    }
    EXPECT_TRUE(cpu_owner.expired());
    ASSERT_EQ(native.generated.size(), 3u);
    const auto unadopted = native.generated.back();
    EXPECT_EQ(unadopted.first, GLResourceKind::Buffer);
    std::thread release([owner = std::move(retained)]() mutable { owner.reset(); });
    release.join();
    EXPECT_TRUE(native.deleted.empty());
    EXPECT_THROW(native.collect(), CE::Exceptions::failed_operation);
    EXPECT_TRUE(native.deleted.empty());
    native.set_current(true);
    native.lose_context_on_error = false;
    native.fail_generation.reset();
    native.collect();
    auto expected = native.generated;
    expected.pop_back(); // The lost unadopted ID belongs to native context destruction.
    auto deleted = native.deleted;
    std::sort(expected.begin(), expected.end());
    std::sort(deleted.begin(), deleted.end());
    EXPECT_EQ(deleted, expected);
    EXPECT_EQ(std::count(native.deleted.begin(), native.deleted.end(), unadopted), 0);
    auto late = native.image();
    native.lifetime()->shutdown();
    const auto after_shutdown = native.deleted.size();
    EXPECT_THROW(late->bind(0), CE::Exceptions::failed_operation);
    late.reset();
    EXPECT_EQ(native.deleted.size(), after_shutdown);
}

TEST(opengl_lifetime, upload_context_loss) {
    for (const bool geometry : {false, true}) {
        SCOPED_TRACE(geometry ? "geometry" : "image");
        NativeProgramRecorder native;
        auto retained_image = native.image();
        auto retained_geometry = native.geometry();
        native.lose_context_on_error = true;
        if (geometry)
            native.fail_buffer_upload = native.buffer_uploads + 1;
        else
            native.fail_image_upload = true;
        if (geometry)
            EXPECT_THROW((void)native.geometry(), CE::Exceptions::failed_operation);
        else
            EXPECT_THROW((void)native.image(), CE::Exceptions::failed_operation);
        EXPECT_TRUE(native.deleted.empty());
        EXPECT_THROW(native.collect(), CE::Exceptions::failed_operation);
        EXPECT_THROW(retained_image->bind(0), CE::Exceptions::failed_operation);
        EXPECT_THROW(retained_geometry->bind(), CE::Exceptions::failed_operation);
        auto lifetime = native.lifetime();
        lifetime->abandon();
        const auto before_late_release = native.current_queries();
        std::thread release([image = std::move(retained_image), vao = std::move(retained_geometry)]() mutable {
            image.reset();
            vao.reset();
        });
        release.join();
        EXPECT_THROW(lifetime->collect(), CE::Exceptions::failed_operation);
        EXPECT_EQ(native.current_queries(), before_late_release);
        EXPECT_TRUE(native.deleted.empty()); // Context destruction owns every adopted native ID.
    }
}

TEST(opengl_pipeline, two_image_effect) {
    NativeProgramRecorder native;
    native.uniforms = {{"uTime", GL_FLOAT, 1, 5}, {"uColor", GL_FLOAT_VEC4, 1, 6}, {"uCamera", GL_FLOAT_MAT4, 1, 7},
        {"uBase", GL_SAMPLER_2D, 1, 8}, {"uMask", GL_SAMPLER_2D, 1, 9}};
    auto definition = time_definition();
    definition.parameters.push_back({"color", ParameterType::Vec4});
    definition.parameters.push_back({"camera", ParameterType::Mat4, true, ParameterSemantic::Projection});
    definition.parameters.push_back({"base", ParameterType::Sampler2D});
    definition.parameters.push_back({"mask", ParameterType::Sampler2D});
    GLSLPipelineBindings bindings{{{"time", "uTime"}, {"color", "uColor"}, {"camera", "uCamera"}, {"base", "uBase"}, {"mask", "uMask"}}};
    auto pipeline = std::make_shared<GLSLPipeline>(definition, native.program(), bindings);
    auto first = native.image();
    auto second = native.image();
    Material material({pipeline, {{"base", ImageBinding{first, 1}}, {"mask", ImageBinding{second, 3}}}});
    ShaderPass camera;
    camera.projection[3][0] = 17.0f;
    auto values = material.resolve(camera, {}, {{"time", 4.0f}}, {{"color", glm::vec4{0.1f, 0.2f, 0.3f, 0.4f}}});
    native.image_binds.clear();
    pipeline->bind_parameters(values);
    EXPECT_EQ(native.uses, 1);
    EXPECT_FLOAT_EQ(std::get<float>(native.writes.at(5)), 4.0f);
    EXPECT_FLOAT_EQ(std::get<glm::vec4>(native.writes.at(6)).w, 0.4f);
    EXPECT_FLOAT_EQ(std::get<glm::mat4>(native.writes.at(7))[3][0], 17.0f);
    EXPECT_EQ(std::get<int>(native.writes.at(8)), 1);
    EXPECT_EQ(std::get<int>(native.writes.at(9)), 3);
    ASSERT_EQ(native.image_binds.size(), 2u);
    EXPECT_EQ(native.image_binds[0].first, 1u);
    EXPECT_EQ(native.image_binds[1].first, 3u);
    EXPECT_NE(native.image_binds[0].second, native.image_binds[1].second);
}

TEST(opengl_pipeline, optional_uniform_reset) {
    NativeProgramRecorder native;
    native.uniforms = {{"uTime", GL_FLOAT, 1, 5}};
    const auto program = native.program();
    const auto definition = time_definition(false);
    EXPECT_THROW(static_cast<void>(GLSLPipeline(definition, program, {{{"time", "uTime"}}})), invalid_args);
    GLSLPipeline pipeline(definition, program, {{{"time", "uTime", 0.0f}}});
    pipeline.bind_parameters({{"time", 7.0f}});
    pipeline.bind_parameters({});
    EXPECT_EQ(native.uses, 2);
    EXPECT_FLOAT_EQ(std::get<float>(native.writes.at(5)), 0.0f);
}

TEST(opengl_pipeline, inactive_optional_uniforms) {
    NativeProgramRecorder native;
    GLSLPipeline pipeline(time_definition(false), native.program(), {{{"time", "uTime"}}});
    pipeline.bind_parameters({});
    EXPECT_EQ(native.uses, 1);
    EXPECT_TRUE(native.writes.empty());
}

TEST(opengl_pipeline, invalid_uniform_contract) {
    NativeProgramRecorder native;
    const auto program = native.program();
    const auto definition = time_definition();
    const GLSLPipelineBindings bindings{{{"time", "uTime"}}};
    EXPECT_THROW(static_cast<void>(GLSLPipeline(definition, program, bindings)), invalid_args);
    native.uniforms = {{"uTime", GL_INT, 1, 5}};
    EXPECT_THROW(static_cast<void>(GLSLPipeline(definition, program, bindings)), invalid_args);
    native.uniforms = {{"uTime", GL_FLOAT, 2, 5}};
    EXPECT_THROW(static_cast<void>(GLSLPipeline(definition, program, bindings)), invalid_args);
    native.uniforms = {{"uTime", GL_FLOAT, 1, -1}};
    EXPECT_THROW(static_cast<void>(GLSLPipeline(definition, program, bindings)), invalid_args);
    native.uniforms = {{"uTime", GL_FLOAT, 1, 5}, {"outside", GL_FLOAT, 1, 6}};
    EXPECT_THROW(static_cast<void>(GLSLPipeline(definition, program, bindings)), invalid_args);
    EXPECT_EQ(native.uses, 0);
    EXPECT_TRUE(native.writes.empty());
}

TEST(opengl_pipeline, invalid_program_attributes) {
    NativeProgramRecorder native;
    native.uniforms = {{"uTime", GL_FLOAT, 1, 5}};
    const auto program = native.program();
    native.attributes[0].location = 2;
    EXPECT_THROW(static_cast<void>(GLSLPipeline(time_definition(), program, {{{"time", "uTime"}}})), invalid_args);
    native.attributes[0].location = 0;
    native.linked = false;
    EXPECT_THROW(static_cast<void>(GLSLPipeline(time_definition(), program, {{{"time", "uTime"}}})), CE::Exceptions::failed_operation);
    EXPECT_EQ(native.uses, 0);
}

TEST(opengl_pipeline, invalid_sampler_bindings) {
    NativeProgramRecorder native;
    native.uniforms = {{"uTime", GL_FLOAT, 1, 5}, {"uImage", GL_SAMPLER_2D, 1, 6}};
    auto definition = time_definition();
    definition.parameters.push_back({"image", ParameterType::Sampler2D});
    GLSLPipeline pipeline(definition, native.program(), {{{"time", "uTime"}, {"image", "uImage"}}});
    auto foreign = native.image(true);
    auto local = native.image();
    native.image_binds.clear();
    EXPECT_THROW(pipeline.bind_parameters({{"time", 1.0f}, {"image", ImageBinding{foreign, 0}}}), invalid_args);
    EXPECT_THROW(pipeline.bind_parameters({{"time", 1.0f}, {"image", ImageBinding{local, 8}}}), invalid_args);
    EXPECT_THROW(pipeline.bind_parameters({{"time", 1}, {"image", ImageBinding{local, 0}}}), invalid_args);
    EXPECT_EQ(native.uses, 0);
    EXPECT_TRUE(native.writes.empty());
    EXPECT_TRUE(native.image_binds.empty());
}

TEST(opengl_pipeline, per_draw_state) {
    NativeProgramRecorder native;
    native.uniforms = {{"uTime", GL_FLOAT, 1, 5}};
    auto geometry = native.geometry();
    auto definition = time_definition();
    definition.state = {BlendMode::Opaque, DepthMode::LessEqual, true, CullMode::Back};
    GLSLPipeline solid(definition, native.program(), {{{"time", "uTime"}}});
    PassConstraints2D world;
    world.depth = DepthMode::LessEqual;
    solid.draw(*geometry, 0, 6, {{"time", 1.0f}}, world);
    EXPECT_FALSE(native.enabled.at(GL_BLEND));
    EXPECT_TRUE(native.enabled.at(GL_DEPTH_TEST));
    EXPECT_TRUE(native.enabled.at(GL_CULL_FACE));
    EXPECT_EQ(native.depth_function, GL_LEQUAL);
    EXPECT_EQ(native.depth_write, GL_TRUE);
    EXPECT_EQ(native.cull_face, GL_BACK);

    definition.state = {};
    GLSLPipeline overlay(definition, native.program(), {{{"time", "uTime"}}});
    overlay.draw(*geometry, 0, 3, {{"time", 2.0f}}, {});
    EXPECT_TRUE(native.enabled.at(GL_BLEND));
    EXPECT_FALSE(native.enabled.at(GL_DEPTH_TEST));
    EXPECT_FALSE(native.enabled.at(GL_CULL_FACE));
    EXPECT_EQ(native.depth_function, GL_LESS);
    EXPECT_EQ(native.depth_write, GL_FALSE);
    EXPECT_EQ(native.front_face, GL_CCW);
    EXPECT_EQ(native.blend_equations, (std::pair{GLenum{GL_FUNC_ADD}, GLenum{GL_FUNC_ADD}}));
    EXPECT_EQ(native.blend_factors, (std::array<GLenum, 4>{GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA}));
    EXPECT_EQ(native.draws, (std::vector<std::pair<GLint, GLsizei>>{{0, 6}, {0, 3}}));
}

TEST(opengl_pipeline, clipped_draws) {
    NativeProgramRecorder native;
    native.uniforms = {{"uTime", GL_FLOAT, 1, 5}};
    GLSLPipeline pipeline(time_definition(), native.program(), {{{"time", "uTime"}}});
    auto geometry = native.geometry();
    const ClipRegion2D clip{{10, 20, 60, 80}, 100, 100};
    pipeline.draw(*geometry, 0, 6, {{"time", 1.0f}}, {}, clip, {200, 300});
    EXPECT_TRUE(native.enabled.at(GL_SCISSOR_TEST));
    EXPECT_EQ(native.scissor_box, (std::array<GLint, 4>{20, 60, 100, 180}));
    pipeline.draw(*geometry, 0, 6, {{"time", 1.0f}}, {});
    EXPECT_FALSE(native.enabled.at(GL_SCISSOR_TEST));
    const auto changes = native.state_changes;
    pipeline.draw(*geometry, 0, 6, {{"time", 1.0f}}, {}, ClipRegion2D{{2, 0, 2, 10}, 100, 100}, {200, 300});
    EXPECT_EQ(native.draws.size(), 2u);
    EXPECT_EQ(native.state_changes, changes);
    EXPECT_THROW(pipeline.draw(*geometry, 0, 6, {{"time", 1.0f}}, {}, ClipRegion2D{{}, 0, 100}, {200, 300}), invalid_args);
    EXPECT_EQ(native.state_changes, changes);
}

TEST(opengl_pipeline, colored_vertices) {
    NativeProgramRecorder native;
    native.uniforms = {{"uTime", GL_FLOAT, 1, 5}};
    native.attributes.push_back({"in_Color", GL_FLOAT_VEC4, 1, 2});
    auto definition = time_definition();
    EXPECT_THROW((void)GLSLPipeline(definition, native.program(), {{{"time", "uTime"}}}), invalid_args);
    definition.vertex_layout = VertexLayout2D::Position3UV2Color4;
    GLSLPipeline pipeline(definition, native.program(), {{{"time", "uTime"}}});
    const std::array<CE::Vertex2DColor, 6> vertices{};
    CE::VAO geometry(native.lifetime(), vertices, PrimitiveTopology::Triangles);
    EXPECT_EQ(geometry.vertex_layout(), VertexLayout2D::Position3UV2Color4);
    EXPECT_EQ(native.attribute_layouts.at(2), (std::pair<GLint, GLsizei>{4, sizeof(CE::Vertex2DColor)}));
    pipeline.draw(geometry, 0, 6, {{"time", 1.0f}}, {});
    native.attributes.back().type = GL_FLOAT_VEC3;
    EXPECT_THROW((void)GLSLPipeline(definition, native.program(), {{{"time", "uTime"}}}), invalid_args);
}

TEST(opengl_pipeline, draw_validation) {
    NativeProgramRecorder native;
    native.uniforms = {{"uTime", GL_FLOAT, 1, 5}};
    GLSLPipeline pipeline(time_definition(), native.program(), {{{"time", "uTime"}}});
    auto local = native.geometry();
    auto foreign = native.geometry(true);
    native.geometry_binds = 0;
    EXPECT_THROW(pipeline.draw(*foreign, 0, 6, {{"time", 1.0f}}, {}), invalid_args);
    EXPECT_THROW(pipeline.draw(*local, 4, 3, {{"time", 1.0f}}, {}), invalid_args);
    EXPECT_THROW(pipeline.draw(*local, 0, 6, {{"time", 1}}, {}), invalid_args);
    PassConstraints2D world;
    world.depth = DepthMode::Less;
    EXPECT_THROW(pipeline.draw(*local, 0, 6, {{"time", 1.0f}}, world), invalid_args);
    EXPECT_EQ(native.uses, 0);
    EXPECT_EQ(native.state_changes, 0);
    EXPECT_EQ(native.geometry_binds, 0);
    EXPECT_TRUE(native.draws.empty());
}

TEST(opengl_texture, explicit_unbind_unit) {
    NativeProgramRecorder native;
    auto first = native.image();
    auto second = native.image();
    first->bind(1);
    second->bind(3);
    native.image_binds.clear();
    first->unbind(1);
    EXPECT_EQ(native.image_binds, (std::vector<std::pair<std::uint32_t, GLuint>>{{1, 0}}));
}

TEST(opengl_sampler, filter_modes) {
    NativeProgramRecorder native;
    const std::array<GLint, 6> expected{GL_NEAREST, GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR,
                                      GL_LINEAR, GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR};
    std::size_t index = 0;
    for (const auto filter : {ImageFilter::Nearest, ImageFilter::Linear}) {
        for (const auto mipmaps : {MipmapFilter::None, MipmapFilter::Nearest, MipmapFilter::Linear}) {
            SamplerOptions options;
            options.minification = filter;
            options.magnification = filter;
            options.mipmaps = mipmaps;
            options.anisotropy = ImageAnisotropy::Disabled;
            const auto sampler = native.sampler(options);
            sampler->bind(3);
            const auto id = native.sampler_binds.back().second;
            const auto& parameters = native.sampler_parameters.at(id);
            EXPECT_EQ(parameters.at(GL_TEXTURE_MIN_FILTER), expected[index++]);
            EXPECT_EQ(parameters.at(GL_TEXTURE_MAG_FILTER), filter == ImageFilter::Nearest ? GL_NEAREST : GL_LINEAR);
            EXPECT_EQ(parameters.at(GL_TEXTURE_WRAP_S), GL_CLAMP_TO_EDGE);
            EXPECT_EQ(parameters.at(GL_TEXTURE_WRAP_T), GL_CLAMP_TO_EDGE);
        }
    }
    EXPECT_EQ(native.anisotropy_queries, 0);
    EXPECT_TRUE(native.sampler_anisotropy.empty());
}

TEST(opengl_sampler, independent_modes) {
    NativeProgramRecorder native;
    SamplerOptions options;
    options.minification = ImageFilter::Nearest;
    options.magnification = ImageFilter::Linear;
    options.mipmaps = MipmapFilter::Linear;
    options.wrap_u = ImageWrap::Repeat;
    options.wrap_v = ImageWrap::MirroredRepeat;
    options.anisotropy = ImageAnisotropy::Disabled;
    const auto sampler = native.sampler(options);
    sampler->bind(2);
    const auto& parameters = native.sampler_parameters.at(native.sampler_binds.back().second);
    EXPECT_EQ(parameters.at(GL_TEXTURE_MIN_FILTER), GL_NEAREST_MIPMAP_LINEAR);
    EXPECT_EQ(parameters.at(GL_TEXTURE_MAG_FILTER), GL_LINEAR);
    EXPECT_EQ(parameters.at(GL_TEXTURE_WRAP_S), GL_REPEAT);
    EXPECT_EQ(parameters.at(GL_TEXTURE_WRAP_T), GL_MIRRORED_REPEAT);
}

TEST(opengl_sampler, anisotropy_defaults) {
    for (const int support : {-1, 0, 1, 2}) {
        SCOPED_TRACE(support);
        NativeProgramRecorder native;
        if (support >= 0)
            native.enable_anisotropy(support);
        const auto sampler = native.sampler({});
        sampler->bind(0);
        const auto id = native.sampler_binds.back().second;
        EXPECT_FLOAT_EQ(sampler->effective_anisotropy(), support < 0 ? 1 : 16);
        EXPECT_EQ(sampler->options().anisotropy, ImageAnisotropy::MaximumSupported);
        EXPECT_EQ(native.anisotropy_queries, support < 0 ? 0 : 1);
        if (support >= 0)
            EXPECT_FLOAT_EQ(native.sampler_anisotropy.at(id), 16);
        else
            EXPECT_TRUE(native.sampler_anisotropy.empty());
        static_cast<void>(native.image());
        if (support >= 0)
            EXPECT_FLOAT_EQ(native.image_anisotropy, 16);
    }
}

TEST(opengl_sampler, anisotropy_disabled) {
    NativeProgramRecorder native;
    native.enable_anisotropy();
    native.fail_anisotropy_query = true;
    SamplerOptions options;
    options.minification = options.magnification = ImageFilter::Nearest;
    options.mipmaps = MipmapFilter::None;
    options.anisotropy = ImageAnisotropy::Disabled;
    const auto sampler = native.sampler(options);
    sampler->bind(1);
    EXPECT_FLOAT_EQ(sampler->effective_anisotropy(), 1);
    EXPECT_FLOAT_EQ(native.sampler_anisotropy.at(native.sampler_binds.back().second), 1);
    EXPECT_EQ(native.anisotropy_queries, 0);
    const unsigned char pixels[]{255, 255, 255, 255};
    const Texture image(native.lifetime(), pixels, 1, 1, true, true, GL_CLAMP_TO_EDGE, GL_RGBA);
    EXPECT_EQ(native.image_parameters.at(GL_TEXTURE_MIN_FILTER), GL_NEAREST_MIPMAP_NEAREST);
    EXPECT_EQ(native.image_parameters.at(GL_TEXTURE_MAG_FILTER), GL_NEAREST);
    EXPECT_FLOAT_EQ(native.image_anisotropy, 1);
    EXPECT_EQ(native.anisotropy_queries, 0);
}

TEST(opengl_sampler, cache_lifetime) {
    NativeProgramRecorder native;
    RecordingContext context(native);
    OpenGLRenderer renderer(context);
    RendererDetail::RendererAccess::set_native_loader(renderer, [](iOpenGLContext&) {});
    renderer.initialize();
    OpenGLResourceProvider provider(renderer);
    auto first = provider.create_sampler({});
    auto second = provider.create_sampler({});
    EXPECT_EQ(first, second);
    auto worker = std::async(std::launch::async, [&] {
        EXPECT_THROW(static_cast<void>(provider.create_sampler({})), CE::Exceptions::failed_operation);
    });
    worker.get();
    ASSERT_EQ(native.generated.size(), 1u);
    const auto original = native.generated.front();
    SamplerOptions nearest;
    nearest.minification = nearest.magnification = ImageFilter::Nearest;
    nearest.mipmaps = MipmapFilter::None;
    nearest.anisotropy = ImageAnisotropy::Disabled;
    const auto different = provider.create_sampler(nearest);
    EXPECT_NE(first, different);
    std::weak_ptr<const Sampler> retained = first;
    first.reset();
    second.reset();
    EXPECT_TRUE(retained.expired());
    renderer.maintain_resources();
    EXPECT_EQ(native.deleted, (std::vector<std::pair<GLResourceKind, GLuint>>{original}));
    const auto replacement = provider.create_sampler({});
    EXPECT_EQ(native.generated.size(), 3u);
    EXPECT_EQ(replacement->options(), SamplerOptions{});
    renderer.deinitialize();
    EXPECT_EQ(native.deleted.size(), 3u);
    EXPECT_THROW(static_cast<void>(provider.create_sampler({})), CE::Exceptions::failed_operation);
    EXPECT_THROW(replacement->bind(0), CE::Exceptions::failed_operation);
}

TEST(opengl_sampler, creation_failures) {
    for (const int failure : {0, 1, 2, 3, 4, 5, 6}) {
        SCOPED_TRACE(failure);
        NativeProgramRecorder native;
        if (failure == 0)
            native.pending_error(GL_INVALID_OPERATION);
        else if (failure == 1)
            native.fail_generation = GLResourceKind::Sampler;
        else if (failure == 2)
            native.reject_registration(1);
        else if (failure == 3)
            native.fail_sampler_parameters = true;
        else if (failure == 4)
            native.fail_integer_query = GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS;
        else {
            native.enable_anisotropy();
            native.fail_anisotropy_query = failure == 5;
            native.maximum_anisotropy = std::numeric_limits<float>::quiet_NaN();
        }
        if (failure == 2)
            EXPECT_THROW(static_cast<void>(native.sampler({})), std::bad_alloc);
        else
            EXPECT_THROW(static_cast<void>(native.sampler({})), CE::Exceptions::failed_operation);
        EXPECT_TRUE(native.sampler_binds.empty());
        native.collect();
        EXPECT_EQ(native.deleted, native.generated);
        EXPECT_EQ(native.generated.size(), failure == 0 || failure >= 5 ? 0u : 1u);
    }
}

TEST(opengl_sampler, binding_guards) {
    NativeProgramRecorder native;
    const auto sampler = native.sampler({});
    EXPECT_THROW(sampler->bind(8), invalid_args);
    auto worker = std::async(std::launch::async, [&] {
        EXPECT_THROW(sampler->bind(0), CE::Exceptions::failed_operation);
    });
    worker.get();
    native.set_current(false);
    EXPECT_THROW(sampler->bind(0), CE::Exceptions::failed_operation);
    native.set_current(true);
    native.lifetime()->shutdown();
    EXPECT_THROW(sampler->bind(0), CE::Exceptions::failed_operation);
    EXPECT_TRUE(native.sampler_binds.empty());
}

TEST(opengl_pipeline, sampler_isolation) {
    NativeProgramRecorder native;
    native.uniforms = {{"uImage", GL_SAMPLER_2D, 1, 6}};
    auto definition = time_definition();
    definition.parameters = {{"image", ParameterType::Sampler2D}};
    GLSLPipeline pipeline(definition, native.program(), {{{"image", "uImage"}}});
    auto image = native.image();
    SamplerOptions nearest;
    nearest.minification = nearest.magnification = ImageFilter::Nearest;
    nearest.mipmaps = MipmapFilter::None;
    nearest.anisotropy = ImageAnisotropy::Disabled;
    const auto first = native.sampler(nearest);
    const auto second = native.sampler({});
    const auto parameters = native.image_parameters;
    const auto uploads = native.image_uploads;
    native.sampler_binds.clear();
    pipeline.bind_parameters({{"image", ImageBinding{image, 4, first}}});
    ASSERT_EQ(native.sampler_binds.size(), 2u);
    const auto nearest_id = native.sampler_binds.back().second;
    pipeline.bind_parameters({{"image", ImageBinding{image, 4, second}}});
    const auto smooth_id = native.sampler_binds.back().second;
    pipeline.bind_parameters({{"image", ImageBinding{image, 4}}});
    pipeline.bind_parameters({{"image", ImageBinding{image, 4, first}}});
    EXPECT_NE(nearest_id, smooth_id);
    EXPECT_EQ(
        native.sampler_binds,
        (std::vector<std::pair<std::uint32_t, GLuint>>{{4, 0}, {4, nearest_id}, {4, 0}, {4, smooth_id}, {4, 0}, {4, 0}, {4, nearest_id}})
    );
    EXPECT_EQ(native.image_uploads, uploads);
    EXPECT_EQ(native.image_parameters, parameters);
}

TEST(opengl_pipeline, sampler_validation) {
    NativeProgramRecorder native;
    native.uniforms = {{"uImage", GL_SAMPLER_2D, 1, 6}};
    auto definition = time_definition();
    definition.parameters = {{"image", ParameterType::Sampler2D}};
    GLSLPipeline pipeline(definition, native.program(), {{{"image", "uImage"}}});
    const auto geometry = native.geometry();
    const auto image = native.image();
    const auto foreign = native.sampler({}, true);
    const std::array<unsigned char, 16> pixels{};
    const auto incomplete = std::make_shared<Texture>(native.lifetime(), pixels.data(), 2, 2, false, false, GL_CLAMP_TO_EDGE, GL_RGBA);
    const auto mipmapped = native.sampler({});
    native.image_binds.clear();
    native.sampler_binds.clear();
    const auto state_changes = native.state_changes;
    EXPECT_THROW(pipeline.draw(*geometry, 0, 3, {{"image", ImageBinding{image, 0, foreign}}}, {}), invalid_args);
    EXPECT_THROW(pipeline.draw(*geometry, 0, 3, {{"image", ImageBinding{incomplete, 0, mipmapped}}}, {}), invalid_args);
    EXPECT_EQ(native.uses, 0);
    EXPECT_EQ(native.state_changes, state_changes);
    EXPECT_TRUE(native.image_binds.empty());
    EXPECT_TRUE(native.sampler_binds.empty());
    EXPECT_TRUE(native.writes.empty());
    EXPECT_TRUE(native.draws.empty());
    SamplerOptions base_level;
    base_level.mipmaps = MipmapFilter::None;
    const auto nonmipmapped = native.sampler(base_level);
    EXPECT_NO_THROW(pipeline.bind_parameters({{"image", ImageBinding{incomplete, 0, nonmipmapped}}}));
    EXPECT_NO_THROW(pipeline.bind_parameters({{"image", ImageBinding{image, 0, mipmapped}}})); // A 1x1 image is already complete.
}

TEST(opengl_pipeline, sampler_reset) {
    NativeProgramRecorder native;
    native.uniforms = {{"uImage", GL_SAMPLER_2D, 1, 6}};
    auto definition = time_definition();
    definition.parameters = {{"image", ParameterType::Sampler2D, false}};
    const auto image = native.image();
    GLSLPipeline pipeline(definition, native.program(), {{{"image", "uImage", ImageBinding{image, 4}}}});
    SamplerOptions nearest;
    nearest.minification = nearest.magnification = ImageFilter::Nearest;
    nearest.mipmaps = MipmapFilter::None;
    nearest.anisotropy = ImageAnisotropy::Disabled;
    const auto sampler = native.sampler(nearest);
    native.sampler_binds.clear();
    pipeline.bind_parameters({{"image", ImageBinding{image, 4, sampler}}});
    ASSERT_EQ(native.sampler_binds.size(), 2u);
    EXPECT_NE(native.sampler_binds.back().second, 0u);
    pipeline.bind_parameters({});
    ASSERT_EQ(native.sampler_binds.size(), 3u);
    EXPECT_EQ(native.sampler_binds.back(), (std::pair<std::uint32_t, GLuint>{4, 0}));
    EXPECT_EQ(std::get<int>(native.writes.at(6)), 4);
}

TEST(opengl_upload, buffer_upload_failure) {
    NativeProgramRecorder native;
    native.fail_buffer_upload = 1;
    EXPECT_THROW(static_cast<void>(native.geometry()), CE::Exceptions::failed_operation);
    ASSERT_EQ(native.generated.size(), 2u);
    EXPECT_EQ(native.buffer_uploads, 1);
    EXPECT_EQ(native.attributes_enabled, 0);
    EXPECT_TRUE(native.deleted.empty());
    native.collect();
    auto generated = native.generated;
    auto deleted = native.deleted;
    std::sort(generated.begin(), generated.end());
    std::sort(deleted.begin(), deleted.end());
    EXPECT_EQ(deleted, generated);
    native.collect();
    EXPECT_EQ(native.deleted.size(), 2u);
}

TEST(opengl_upload, texture_registry_failure) {
    NativeProgramRecorder native;
    native.reject_registration(1);
    EXPECT_THROW((void)native.image(), std::bad_alloc);
    EXPECT_EQ(native.rejected_allocations(), 1u);
    EXPECT_EQ(native.image_uploads, 0);
    ASSERT_EQ(native.generated.size(), 1u);
    EXPECT_EQ(native.deleted, native.generated);
    native.collect();
    EXPECT_EQ(native.deleted.size(), 1u);
}

TEST(opengl_upload, geometry_registry_failure) {
    for (const int registration : {1, 2}) {
        SCOPED_TRACE(registration);
        NativeProgramRecorder native;
        native.reject_registration(registration);
        EXPECT_THROW((void)native.geometry(), std::bad_alloc);
        EXPECT_EQ(native.rejected_allocations(), 1u);
        EXPECT_EQ(native.buffer_uploads, 0);
        ASSERT_EQ(native.deleted.size(), 1u); // The current ID was never adopted.
        native.release_registration_padding();
        native.collect();
        auto expected = native.generated;
        auto actual = native.deleted;
        std::sort(expected.begin(), expected.end());
        std::sort(actual.begin(), actual.end());
        EXPECT_EQ(actual, expected); // Earlier adopted IDs retire with the failed object.
        native.collect();
        EXPECT_EQ(native.deleted.size(), expected.size());
    }
}

TEST(opengl_upload, mesh_registry_failure) {
    for (const int registration : {1, 2, 3}) {
        SCOPED_TRACE(registration);
        NativeProgramRecorder native;
        native.reject_registration(registration);
        auto vertices = std::shared_ptr<CE::Vertex3D>(new CE::Vertex3D[3]{}, std::default_delete<CE::Vertex3D[]>{});
        auto indices = std::shared_ptr<std::uint32_t>(new std::uint32_t[3]{}, std::default_delete<std::uint32_t[]>{});
        EXPECT_THROW((void)CE::VAO(native.lifetime(), vertices, 3, indices, 3), std::bad_alloc);
        EXPECT_EQ(native.rejected_allocations(), 1u);
        EXPECT_EQ(native.buffer_uploads, 0);
        ASSERT_EQ(native.deleted.size(), 1u);
        native.release_registration_padding();
        native.collect();
        auto expected = native.generated;
        auto actual = native.deleted;
        std::sort(expected.begin(), expected.end());
        std::sort(actual.begin(), actual.end());
        EXPECT_EQ(actual, expected);
        native.collect();
        EXPECT_EQ(native.deleted.size(), expected.size());
    }
}

TEST(opengl_upload, anisotropy_query_failure) {
    NativeProgramRecorder native;
    native.enable_anisotropy();
    native.fail_anisotropy_query = true;
    const unsigned char pixels[]{255, 255, 255, 255};
    EXPECT_THROW((void)Texture(native.lifetime(), pixels, 1, 1, true, false, GL_CLAMP_TO_EDGE, GL_RGBA), CE::Exceptions::failed_operation);
    EXPECT_EQ(native.anisotropy_writes, 0);
    EXPECT_EQ(native.image_uploads, 0);
    EXPECT_EQ(native.mipmap_calls, 0);
    EXPECT_TRUE(native.deleted.empty());
    native.collect();
    ASSERT_EQ(native.generated.size(), 1u);
    EXPECT_EQ(native.deleted, native.generated);
    native.collect();
    EXPECT_EQ(native.deleted.size(), 1u);
}

TEST(opengl_upload, texture_query_failure) {
    for (const GLenum parameter : {GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, GL_UNPACK_ALIGNMENT}) {
        SCOPED_TRACE(parameter);
        NativeProgramRecorder native;
        native.fail_integer_query = parameter;
        native.unpack_alignment = 8;
        const unsigned char alpha[]{255};
        EXPECT_THROW(
            (void)Texture(native.lifetime(), alpha, 1, 1, false, false, GL_CLAMP_TO_EDGE, GL_RED), CE::Exceptions::failed_operation
        );
        EXPECT_EQ(native.image_uploads, 0);
        EXPECT_EQ(native.unpack_alignment, 8);
        EXPECT_TRUE(native.deleted.empty());
        native.collect();
        ASSERT_EQ(native.generated.size(), 1u);
        EXPECT_EQ(native.deleted, native.generated);
        native.collect();
        EXPECT_EQ(native.deleted.size(), 1u);
    }
}

TEST(opengl_upload, mesh_upload_failure) {
    for (const int failed_upload : {1, 2}) {
        NativeProgramRecorder native;
        native.fail_buffer_upload = failed_upload;
        auto vertices = std::shared_ptr<CE::Vertex3D>(new CE::Vertex3D[3]{}, std::default_delete<CE::Vertex3D[]>{});
        auto indices = std::shared_ptr<std::uint32_t>(new std::uint32_t[3]{}, std::default_delete<std::uint32_t[]>{});
        EXPECT_THROW(static_cast<void>(CE::VAO(native.lifetime(), vertices, 3, indices, 3)), CE::Exceptions::failed_operation);
        ASSERT_EQ(native.generated.size(), 3u);
        EXPECT_EQ(native.buffer_uploads, failed_upload);
        EXPECT_EQ(native.attributes_enabled, 0);
        EXPECT_TRUE(native.deleted.empty());
        native.collect();
        auto generated = native.generated;
        auto deleted = native.deleted;
        std::sort(generated.begin(), generated.end());
        std::sort(deleted.begin(), deleted.end());
        EXPECT_EQ(deleted, generated);
        native.collect();
        EXPECT_EQ(native.deleted.size(), 3u);
    }
}

TEST(opengl_upload, atlas_upload_failure) {
    NativeProgramRecorder native;
    native.fail_image_upload = true;
    const unsigned char pixels[]{255, 255, 255};
    EXPECT_THROW(
        static_cast<void>(Texture(native.lifetime(), pixels, 3, 1, true, false, GL_CLAMP_TO_EDGE, GL_RED)), CE::Exceptions::failed_operation
    );
    EXPECT_EQ(native.unpack_alignment, 4);
    EXPECT_EQ(native.mipmap_calls, 0);
    ASSERT_EQ(native.generated.size(), 1u);
    EXPECT_TRUE(native.deleted.empty());
    native.collect();
    EXPECT_EQ(native.deleted, native.generated);
    native.collect();
    EXPECT_EQ(native.deleted.size(), 1u);
}

TEST(opengl_upload, mipmap_generation_failure) {
    NativeProgramRecorder native;
    native.fail_mipmaps = true;
    const unsigned char pixels[]{255, 255, 255, 255};
    EXPECT_THROW(
        static_cast<void>(Texture(native.lifetime(), pixels, 1, 1, true, false, GL_CLAMP_TO_EDGE, GL_RGBA)),
        CE::Exceptions::failed_operation
    );
    EXPECT_EQ(native.mipmap_calls, 1);
    EXPECT_TRUE(native.deleted.empty());
    native.collect();
    ASSERT_EQ(native.generated.size(), 1u);
    EXPECT_EQ(native.deleted, native.generated);
}

TEST(opengl_upload, pending_creation_errors) {
    NativeProgramRecorder native;
    native.pending_error(GL_INVALID_OPERATION);
    EXPECT_THROW(static_cast<void>(native.image()), CE::Exceptions::failed_operation);
    native.pending_error(GL_INVALID_OPERATION);
    EXPECT_THROW(static_cast<void>(native.geometry()), CE::Exceptions::failed_operation);
    EXPECT_TRUE(native.generated.empty());
    EXPECT_TRUE(native.deleted.empty());
}

TEST(opengl_upload, generation_failure_cleanup) {
    for (const auto kind : {GLResourceKind::Texture, GLResourceKind::VertexArray, GLResourceKind::Buffer}) {
        NativeProgramRecorder native;
        native.fail_generation = kind;
        if (kind == GLResourceKind::Texture) {
            EXPECT_THROW(static_cast<void>(native.image()), CE::Exceptions::failed_operation);
        } else {
            EXPECT_THROW(static_cast<void>(native.geometry()), CE::Exceptions::failed_operation);
        }
        ASSERT_FALSE(native.generated.empty());
        ASSERT_EQ(native.deleted.size(), 1u);
        EXPECT_EQ(native.deleted.front(), native.generated.back());
        EXPECT_EQ(native.buffer_uploads, 0);
        native.collect();
        auto generated = native.generated;
        auto deleted = native.deleted;
        std::sort(generated.begin(), generated.end());
        std::sort(deleted.begin(), deleted.end());
        EXPECT_EQ(deleted, generated);
        native.collect();
        EXPECT_EQ(native.deleted.size(), native.generated.size());
    }
}

TEST(opengl_texture, unbind_context_guards) {
    NativeProgramRecorder native;
    bool current = true;
    int queries = 0;
    auto lifetime = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [&] {
        ++queries;
        return current;
    });
    const unsigned char pixels[]{255, 255, 255, 255};
    Texture image(lifetime, pixels, 1, 1, false, false, GL_CLAMP_TO_EDGE, GL_RGBA);
    native.image_binds.clear();
    const auto before_foreign = queries;
    auto foreign = std::async(std::launch::async, [&] { EXPECT_THROW(image.unbind(1), CE::Exceptions::failed_operation); });
    foreign.get();
    EXPECT_EQ(queries, before_foreign);
    EXPECT_TRUE(native.image_binds.empty());
    current = false;
    EXPECT_THROW(image.unbind(1), CE::Exceptions::failed_operation);
    current = true;
    EXPECT_THROW(image.unbind(8), invalid_args);
    EXPECT_TRUE(native.image_binds.empty());
    lifetime->abandon();
    current = false;
    const auto before_closed = queries;
    EXPECT_THROW(image.unbind(1), CE::Exceptions::failed_operation);
    EXPECT_EQ(queries, before_closed);
    EXPECT_TRUE(native.image_binds.empty());
}
