#include <backends/opengl/program-builder.h>
#include <backends/opengl/pipeline.h>

#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
    using namespace CE::Assets;
    using namespace CE::RenderAPIs;
    using CE::Exceptions::failed_operation;

    // Models attached, delete-marked stages as distinct from their deletion calls.
    // This is source-prepared fault coverage, not a real driver/context test.
    class ProgramConstructionRecorder final {
        inline static ProgramConstructionRecorder* active_ = nullptr;
        std::vector<std::function<void()>> restore_;
        std::map<GLuint, std::set<GLuint>> attachments_;
        std::set<GLuint> marked_stages_;
        GLuint next_id_ = 10;

        template <typename T>
        void replace(
            T& entry,
            T replacement
        ) {
            const auto old = entry;
            restore_.push_back([&entry, old] { entry = old; });
            entry = replacement;
        }

        void restore() noexcept {
            for (auto item = restore_.rbegin(); item != restore_.rend(); ++item)
                (*item)();
            active_ = nullptr;
        }

        bool reject(
            const char* operation
        ) {
            if (fail_operation != operation)
                return false;
            if (lose_current_on_failure)
                current = false;
            error = GL_INVALID_OPERATION;
            return true;
        }

        GLuint generate(
            const GLResourceKind kind,
            const char* operation
        ) {
            const auto id = ++next_id_;
            generated.emplace_back(kind, id);
            (void)reject(operation);
            return id;
        }

        void release_stage(
            const GLuint stage
        ) {
            const bool attached = std::any_of(attachments_.begin(), attachments_.end(), [stage](const auto& entry) {
                return entry.second.contains(stage);
            });
            if (marked_stages_.contains(stage) && !attached) {
                marked_stages_.erase(stage);
                destroyed.emplace_back(GLResourceKind::ShaderStage, stage);
            }
        }

        static GLuint GLAD_API_PTR create_program() {
            const auto id = active_->generate(GLResourceKind::Program, "create program");
            active_->attachments_.emplace(id, std::set<GLuint>{});
            return id;
        }
        static GLuint GLAD_API_PTR create_shader(
            GLenum
        ) {
            return active_->generate(GLResourceKind::ShaderStage, "create shader");
        }
        static void GLAD_API_PTR source(
            GLuint,
            GLsizei,
            const GLchar* const*,
            const GLint*
        ) {
            (void)active_->reject("source");
        }
        static void GLAD_API_PTR compile(
            GLuint
        ) { (void)active_->reject("compile"); }
        static void GLAD_API_PTR shader_query(
            GLuint,
            GLenum parameter,
            GLint* value
        ) {
            if (parameter == GL_COMPILE_STATUS) {
                if (!active_->reject("compile status"))
                    *value = active_->compiled ? GL_TRUE : GL_FALSE;
            }
            else
                *value = 0; // Empty diagnostics for a logical compilation failure.
        }
        static void GLAD_API_PTR attach(
            GLuint program,
            GLuint shader
        ) {
            if (!active_->reject("attach"))
                active_->attachments_.at(program).insert(shader);
        }
        static void GLAD_API_PTR link(
            GLuint
        ) {
            ++active_->link_calls;
            (void)active_->reject("link");
        }
        static void GLAD_API_PTR detach(
            GLuint program,
            GLuint shader
        ) {
            ++active_->detach_calls;
            if (!active_->reject("detach")) {
                active_->attachments_.at(program).erase(shader);
                active_->release_stage(shader);
            }
        }
        static void GLAD_API_PTR program_query(
            GLuint,
            GLenum parameter,
            GLint* value
        ) {
            switch (parameter) {
                case GL_LINK_STATUS:
                    if (!active_->reject("link status"))
                        *value = active_->linked ? GL_TRUE : GL_FALSE;
                    break;
                case GL_ACTIVE_UNIFORMS:
                    if (!active_->reject("reflection count"))
                        *value = 1;
                    break;
                case GL_ACTIVE_UNIFORM_MAX_LENGTH:
                    if (!active_->reject("reflection length"))
                        *value = 6;
                    break;
                case GL_ACTIVE_ATTRIBUTES: *value = 0; break;
                case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH: *value = 1; break;
                default: *value = 0; break;
            }
        }
        static void GLAD_API_PTR uniform_query(
            GLuint,
            GLuint,
            GLsizei,
            GLsizei* written,
            GLint* size,
            GLenum* type,
            GLchar* name
        ) {
            if (active_->reject("reflection entry"))
                return;
            const std::string variable = "uTime";
            std::copy(variable.begin(), variable.end(), name);
            name[variable.size()] = '\0';
            *written = static_cast<GLsizei>(variable.size());
            *size = 1;
            *type = GL_FLOAT;
        }
        static GLint GLAD_API_PTR uniform_location(
            GLuint,
            const GLchar*
        ) {
            return active_->reject("reflection location") ? -1 : 5;
        }
        static void GLAD_API_PTR delete_shader(
            GLuint shader
        ) {
            active_->deletion_calls.emplace_back(GLResourceKind::ShaderStage, shader);
            active_->marked_stages_.insert(shader);
            active_->release_stage(shader);
        }
        static void GLAD_API_PTR delete_program(
            GLuint program
        ) {
            active_->deletion_calls.emplace_back(GLResourceKind::Program, program);
            auto stages = std::move(active_->attachments_.at(program));
            active_->attachments_.erase(program);
            for (const auto shader : stages)
                active_->release_stage(shader);
            active_->destroyed.emplace_back(GLResourceKind::Program, program);
        }
        static GLenum GLAD_API_PTR error_query() { return std::exchange(active_->error, GL_NO_ERROR); }

    public:
        bool current = true;
        bool compiled = true;
        bool linked = true;
        bool lose_current_on_failure = false;
        std::string fail_operation;
        GLenum error = GL_NO_ERROR;
        int link_calls = 0;
        int detach_calls = 0;
        std::vector<std::pair<GLResourceKind, GLuint>> generated;
        std::vector<std::pair<GLResourceKind, GLuint>> deletion_calls;
        std::vector<std::pair<GLResourceKind, GLuint>> destroyed;
        std::shared_ptr<OpenGLResourceLifetime> lifetime =
            std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [this] { return current; });

        ProgramConstructionRecorder() {
            active_ = this;
            try {
                replace(glad_glCreateProgram, &create_program);
                replace(glad_glCreateShader, &create_shader);
                replace(glad_glShaderSource, &source);
                replace(glad_glCompileShader, &compile);
                replace(glad_glGetShaderiv, &shader_query);
                replace(glad_glAttachShader, &attach);
                replace(glad_glLinkProgram, &link);
                replace(glad_glDetachShader, &detach);
                replace(glad_glGetProgramiv, &program_query);
                replace(glad_glGetActiveUniform, &uniform_query);
                replace(glad_glGetUniformLocation, &uniform_location);
                replace(glad_glDeleteShader, &delete_shader);
                replace(glad_glDeleteProgram, &delete_program);
                replace(glad_glGetError, &error_query);
            }
            catch (...) {
                restore();
                throw;
            }
        }

        ~ProgramConstructionRecorder() {
            lifetime->abandon();
            restore();
        }

        static std::vector<std::filesystem::path> stages() {
            const auto root = std::filesystem::path{CHERYL_SOURCE_DIR} / "assets/shaders";
            return {root / "shader2d.vert", root / "shader2d.frag"};
        }

        void expect_all_destroyed_once() const {
            auto expected = generated;
            auto calls = deletion_calls;
            auto actual = destroyed;
            std::sort(expected.begin(), expected.end());
            std::sort(calls.begin(), calls.end());
            std::sort(actual.begin(), actual.end());
            EXPECT_EQ(calls, expected);
            EXPECT_EQ(actual, expected);
            EXPECT_TRUE(attachments_.empty());
            EXPECT_TRUE(marked_stages_.empty());
        }
    };
}

TEST(opengl_program_builder, native_construction_errors_reject_and_release_all_created_objects) {
    const auto stages = ProgramConstructionRecorder::stages();
    for (const std::string operation : {"create program", "create shader", "source", "compile", "compile status",
             "attach", "link", "detach", "link status"}) {
        SCOPED_TRACE(operation);
        ProgramConstructionRecorder native;
        native.fail_operation = operation;
        EXPECT_THROW((void)ProgramDetail::link_program(native.lifetime, stages), failed_operation);
        native.lifetime->collect();
        native.expect_all_destroyed_once();
        if (operation == "attach")
            EXPECT_EQ(native.link_calls, 0); // A permissive link cannot hide the skipped stage.
    }
}

TEST(opengl_program_builder, logical_compile_and_link_failure_release_attached_marked_stages) {
    const auto stages = ProgramConstructionRecorder::stages();
    for (const bool fail_compile : {true, false}) {
        SCOPED_TRACE(fail_compile);
        ProgramConstructionRecorder native;
        native.compiled = !fail_compile;
        native.linked = fail_compile;
        EXPECT_THROW((void)ProgramDetail::link_program(native.lifetime, stages), CE::Exceptions::runtime_exception);
        native.lifetime->collect();
        native.expect_all_destroyed_once();
    }
}

TEST(opengl_program_builder, unreadable_later_stage_releases_the_already_attached_stage) {
    ProgramConstructionRecorder native;
    auto stages = ProgramConstructionRecorder::stages();
    stages[1] = stages[0] / "missing.frag"; // A regular file cannot contain this child.
    EXPECT_THROW((void)ProgramDetail::link_program(native.lifetime, stages), CE::Exceptions::runtime_exception);
    EXPECT_EQ(native.generated.size(), 2u); // Program plus the first compiled stage.
    native.expect_all_destroyed_once();
}

TEST(opengl_program_builder, existing_error_or_missing_current_context_reject_before_creation) {
    ProgramConstructionRecorder native;
    const auto stages = ProgramConstructionRecorder::stages();
    native.error = GL_INVALID_OPERATION;
    EXPECT_THROW((void)ProgramDetail::link_program(native.lifetime, stages), failed_operation);
    EXPECT_TRUE(native.generated.empty());
    native.current = false;
    EXPECT_THROW((void)ProgramDetail::link_program(native.lifetime, stages), failed_operation);
    EXPECT_TRUE(native.generated.empty());
}

TEST(opengl_program_builder, successful_program_drops_stages_but_retained_owner_delays_program_retirement) {
    ProgramConstructionRecorder native;
    auto program = ProgramDetail::link_program(native.lifetime, ProgramConstructionRecorder::stages());
    auto retained = program;
    EXPECT_EQ(native.detach_calls, 2);
    ASSERT_EQ(native.destroyed.size(), 2u);
    for (const auto& [kind, id] : native.destroyed) {
        (void)id;
        EXPECT_EQ(kind, GLResourceKind::ShaderStage);
    }
    program.reset();
    native.lifetime->collect();
    EXPECT_EQ(native.destroyed.size(), 2u);
    std::thread release([owner = std::move(retained)]() mutable { owner.reset(); });
    release.join();
    EXPECT_EQ(native.destroyed.size(), 2u); // The foreign destructor issues no GL call.
    native.lifetime->collect();
    native.expect_all_destroyed_once();
}

TEST(opengl_program_builder, lost_current_context_preserves_construction_failure_and_skips_native_cleanup) {
    ProgramConstructionRecorder native;
    native.fail_operation = "compile";
    native.lose_current_on_failure = true;
    try {
        (void)ProgramDetail::link_program(native.lifetime, ProgramConstructionRecorder::stages());
        FAIL() << "Expected native compilation failure";
    }
    catch (const failed_operation& failure) {
        EXPECT_NE(std::string{failure.what()}.find("Could not compile shader stage"), std::string::npos);
    }
    EXPECT_FALSE(native.current);
    EXPECT_EQ(native.generated.size(), 2u);
    EXPECT_TRUE(native.deletion_calls.empty());
    EXPECT_THROW(native.lifetime->shutdown(), failed_operation);
    native.lifetime->abandon();
    EXPECT_TRUE(native.deletion_calls.empty());
    // Untracked native remnants belong to platform context destruction.
    // This recorder cannot establish actual driver cleanup after context loss.
}

TEST(opengl_program_builder, failed_reflection_rejects_pipeline_and_preserves_retained_program) {
    for (const std::string operation : {"link status", "reflection count", "reflection length", "reflection entry",
             "reflection location"}) {
        SCOPED_TRACE(operation);
        ProgramConstructionRecorder native;
        const auto stages = ProgramConstructionRecorder::stages();
        auto program = ProgramDetail::link_program(native.lifetime, stages);
        native.fail_operation = operation;
        PipelineDefinition definition;
        definition.program_sources = stages;
        // An optional contract makes a failed count query particularly easy to mistake for success.
        definition.parameters = {{"time", ParameterType::Float, false}};
        GLSLPipelineBindings bindings{{{"time", "uTime", ParameterValue{0.0f}}}};
        EXPECT_THROW((void)std::make_shared<GLSLPipeline>(definition, program, bindings), failed_operation);
        native.lifetime->collect();
        EXPECT_EQ(native.destroyed.size(), 2u);
        native.fail_operation.clear();
        auto recovered = std::make_shared<GLSLPipeline>(definition, program, bindings);
        program.reset();
        native.lifetime->collect();
        EXPECT_EQ(native.destroyed.size(), 2u);
        recovered.reset();
        native.lifetime->collect();
        native.expect_all_destroyed_once();
    }
}
