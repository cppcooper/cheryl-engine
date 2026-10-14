#include <assets/resources/resource-provider.h>
#include <assets/resources/shader-asset-builder.h>
#include <assets/submission/draw2d.h>
#include <core/resources/asset-management/asset-loader.h>
#include <core/resources/asset-management/file-registry.h>
#include <core/resources/asset-management/manifest-loader.h>
#include <core/resources/asset-management/shader-asset-mgr.h>
#include <core/resources/asset-management/sprite-mgr.h>
#include <core/resources/asset-management/tileset-mgr.h>
#include <internals/exceptions.h>

#include <gtest/gtest.h>

#include <atomic>
#include <array>
#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>

#ifdef GL_VERSION_3_3
#error CE::Rendering::Resources::Program asset loading must not depend on OpenGL.
#endif

namespace {
    namespace fs = std::filesystem;

    const std::string shader_json = R"JSON({
      "asset_class":"shader","version":"2.0","namespace":"test",
      "programs":[{"name":"program","stages":{"vertex":"shaders/source.vert","fragment":"shaders/source.frag"}}],
      "materials":[{"name":"images","program":"test:program","vertex_layout":"position3_uv2","topology":"triangle_strip",
        "parameters":[{"key":"image","type":"sampler2d"},{"key":"weight","type":"float","default":1.0}],
        "bindings":{"recording":{"parameters":{"image":"image","weight":"weight"}}}}]
    })JSON";

    const std::string grid_json = R"JSON({
      "$schema":"../schemas/graphics/sprite-tileset-manifest-2.0.schema.json",
      "asset_class":"sprite-tileset","version":"2.0","namespace":"test",
      "texture":"pixel.png","shader":"test:images",
      "defaults":{"sprite":{"pivot":{"x":0.5,"y":1}},"tileset":{"pivot":{"x":0.5,"y":0.5}}},
      "sprites":{"pixel":{"grid":{"origin":{"x":0,"y":0},"frame":{"width":1,"height":1},"spacing":{"x":0,"y":0},"rows":1,"columns":1,"cell_order":"row-major"}}},
      "tilesets":{"soil":{"grid":{"origin":{"x":0,"y":0},"frame":{"width":1,"height":1},"spacing":{"x":0,"y":0},"rows":1,"columns":1,"cell_order":"row-major"}}}
    })JSON";

    std::string changed(std::string document, const std::string& from, const std::string& to) {
        const auto found = document.find(from);
        if (found == std::string::npos)
            throw std::logic_error("Fixture text is missing");
        document.replace(found, from.size(), to);
        return document;
    }

    struct TemporaryShaders {
        inline static std::atomic<unsigned int> next{0};
        const std::filesystem::path root =
            std::filesystem::temp_directory_path() /
            ("cheryl-shaders-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + '-' +
             std::to_string(next.fetch_add(1))) /
            "graphics";

        TemporaryShaders() {
            write("graphics-manifests.json", R"JSON({"version":"2.0","manifests":["shader.json"]})JSON");
            write("shader.json", shader_json);
            write("shaders/source.vert", "original vertex");
            write("shaders/source.frag", "original fragment");
        }
        ~TemporaryShaders() {
            std::error_code error;
            std::filesystem::remove_all(root.parent_path(), error);
        }
        void write(const std::filesystem::path& relative, const std::string& bytes) const {
            std::filesystem::create_directories((root / relative).parent_path());
            std::ofstream(root / relative, std::ios::binary) << bytes;
        }
        [[nodiscard]] CE::Assets::Loading::Prepared prepare() const {
            return CE::Assets::Loading::Loader(root).prepare_graphics({"graphics-manifests.json"});
        }
        void grid() const {
            std::filesystem::copy_file(
                std::filesystem::path(CHERYL_SOURCE_DIR) / "projects/modules/graphics/opengl/tests/fixtures/rgba-two-rows.png",
                root / "pixel.png"
            );
            write("grid.json", grid_json);
            write("graphics-manifests.json", R"JSON({"version":"2.0","manifests":["grid.json","shader.json","shader.json"]})JSON");
        }
    };

    struct SnapshotShader final : CE::Rendering::Resources::Program {
        const CE::Assets::Shaders::ProgramRecipe snapshot;

        explicit SnapshotShader(CE::Assets::Shaders::ProgramRecipe recipe)
        : snapshot(std::move(recipe)) {}
        void bind_pass(const CE::Rendering::Parameters::PassSemantics&) override {}
        void bind_draw(const CE::Rendering::Parameters::DrawSemantics&) override {}
        void use() override {}
        void set_uniform_value(const char*, float) override {}
        void set_uniform_value(const char*, int) override {}
        void set_uniform_value(const char*, unsigned int) override {}
        void set_uniform_value(const char*, bool) override {}
        void set_uniform_matrix(const char*, const glm::mat4&) override {}
    };

    struct SnapshotPipeline final : CE::Rendering::Resources::Pipeline {
        const std::shared_ptr<SnapshotShader> program;

        SnapshotPipeline(CE::Rendering::Pipelines::Definition definition, std::shared_ptr<SnapshotShader> executable)
        : CE::Rendering::Resources::Pipeline(std::move(definition)), program(std::move(executable)) {}
    };

    struct MemoryImage final : CE::Rendering::Resources::Image {
        const CE::Rendering::Geometry::PixelSize size;

        explicit MemoryImage(CE::Rendering::Geometry::PixelSize dimensions = {1, 1})
        : size(dimensions) {}
        [[nodiscard]] CE::Rendering::Geometry::PixelSize pixel_size() const override { return size; }
        void bind(std::uint32_t) const override {}
    };

    struct MemoryGeometry final : CE::Rendering::Geometry::Buffer2D {
        const std::size_t count;
        const CE::Rendering::Geometry::Topology primitive;

        MemoryGeometry(std::size_t count, CE::Rendering::Geometry::Topology primitive)
        : count(count), primitive(primitive) {}
        [[nodiscard]] CE::Rendering::Geometry::Layout2D vertex_layout() const noexcept override {
            return CE::Rendering::Geometry::Layout2D::Position3UV2;
        }
        [[nodiscard]] CE::Rendering::Geometry::Topology topology() const noexcept override { return primitive; }
        [[nodiscard]] std::size_t vertex_count() const noexcept override { return count; }
        void bind() const override {}
        void draw(std::size_t, std::size_t) const override {}
    };

    struct MemorySampler final : CE::Rendering::Sampling::Sampler {
        explicit MemorySampler(CE::Rendering::Sampling::Options options)
        : CE::Rendering::Sampling::Sampler(options, 1) {}
        void bind(std::uint32_t) const override {}
    };

    struct RecordingProvider final : CE::Rendering::Resources::Provider,
                                     CE::Assets::Shaders::Builder {
        bool supported = true;
        bool fail_program = false;
        std::string fail_material;
        std::size_t program_builds = 0;
        std::size_t material_builds = 0;
        std::size_t image_uploads = 0;

        [[nodiscard]] CE::Assets::Shaders::Builder* shader_asset_builder() noexcept override { return supported ? this : nullptr; }
        void validate_program(const CE::Assets::Shaders::ProgramRecipe& recipe) const override {
            CE::Assets::Shaders::validate_program(recipe);
        }
        void validate_material(
            const CE::Assets::Shaders::MaterialRecipe& recipe,
            const CE::Assets::Shaders::ProgramRecipe& program
        ) const override {
            CE::Assets::Shaders::validate_material(recipe, program);
        }
        [[nodiscard]] std::shared_ptr<CE::Rendering::Resources::Program>
        build_program(const CE::Assets::Shaders::ProgramRecipe& recipe) override {
            if (fail_program)
                throw CE::Exceptions::failed_operation(CE_HERE, "Simulated program failure");
            ++program_builds;
            return std::make_shared<SnapshotShader>(recipe);
        }
        [[nodiscard]] std::shared_ptr<const CE::Rendering::Resources::Material> build_material(
            const CE::Assets::Shaders::MaterialRecipe& recipe,
            const CE::Assets::Shaders::ProgramRecipe& program,
            const std::shared_ptr<CE::Rendering::Resources::Program>& executable
        ) override {
            if (recipe.id == fail_material)
                throw CE::Exceptions::failed_operation(CE_HERE, "Simulated material failure");
            auto snapshot = std::dynamic_pointer_cast<SnapshotShader>(executable);
            if (!snapshot || snapshot->snapshot.sources[0].bytes != program.sources[0].bytes)
                throw CE::Exceptions::failed_operation(CE_HERE, "Program metadata does not describe its executable");
            ++material_builds;
            CE::Rendering::Materials::Definition definition;
            definition.pipeline = std::make_shared<SnapshotPipeline>(CE::Assets::Shaders::pipeline_definition(recipe, program), snapshot);
            definition.defaults = CE::Assets::Shaders::defaults(recipe);
            for (const auto& [key, options] : recipe.sampling)
                definition.sampling.emplace(key, create_sampler(options));
            return std::make_shared<const CE::Rendering::Resources::Material>(std::move(definition));
        }
        [[nodiscard]] std::shared_ptr<CE::Rendering::Resources::Image> create_image(const CE::Assets::Images::Decoded& image) override {
            ++image_uploads;
            return std::make_shared<MemoryImage>(image.size);
        }
        [[nodiscard]] std::shared_ptr<const CE::Rendering::Sampling::Sampler>
        create_sampler(const CE::Rendering::Sampling::Options& options) override {
            return std::make_shared<MemorySampler>(options);
        }
        [[nodiscard]] std::shared_ptr<CE::Rendering::Resources::Image>
        create_font_atlas(std::span<const unsigned char>, CE::Rendering::Geometry::PixelSize size) override {
            return std::make_shared<MemoryImage>(size);
        }
        [[nodiscard]] std::shared_ptr<CE::Rendering::Geometry::Buffer2D>
        upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2D> vertices, CE::Rendering::Geometry::Topology topology) override {
            return std::make_shared<MemoryGeometry>(vertices.size(), topology);
        }
        [[nodiscard]] std::shared_ptr<CE::Rendering::Resources::Program> link_program(const std::vector<std::filesystem::path>&) override {
            throw CE::Exceptions::failed_operation(CE_HERE, "The indexed route must use owned sources");
        }
    };
}

TEST(shader_assets, selected_sources) {
    TemporaryShaders files;
    files.write("unlisted.json", "malformed JSON");
    files.write("optional.png", "undecodable image");
    files.write("other/source.vert", "wrong basename candidate");
    auto prepared = files.prepare();
    ASSERT_EQ(prepared.shaders.size(), 1u);
    EXPECT_TRUE(prepared.images.empty());
    const auto& source = prepared.shaders.front().programs.front().sources.front();
    EXPECT_EQ(source.path, files.root / "shaders/source.vert");
    EXPECT_EQ(source.bytes, "original vertex");
    EXPECT_THROW(static_cast<void>(CE::Assets::Loading::Loader(files.root).prepare()), CE::Exceptions::runtime_exception);
    files.write("shaders/source.vert", "changed vertex");
    std::filesystem::remove(files.root / "shaders/source.frag");
    RecordingProvider provider;
    CE::Assets::Loading::Loader(files.root).upload(std::move(prepared), provider);
    const auto program = CE::Assets::Shaders::Manager::get().find_program("test:program");
    ASSERT_NE(program, nullptr);
    EXPECT_EQ(program->recipe.sources[0].bytes, "original vertex");
    EXPECT_EQ(program->recipe.sources[1].bytes, "original fragment");
    EXPECT_EQ(provider.program_builds, 1u);
    EXPECT_EQ(provider.image_uploads, 0u);
}

TEST(shader_assets, unsupported_provider) {
    TemporaryShaders files;
    auto prepared = files.prepare();
    RecordingProvider provider;
    provider.supported = false;
    CE::Assets::Loading::Loader loader(files.root);
    EXPECT_THROW(loader.upload(prepared, provider), CE::Exceptions::failed_operation);
    EXPECT_FALSE(CE::Assets::Shaders::Manager::get().contains("test:program"));
    EXPECT_FALSE(CE::Assets::Loading::FileRegistry::get().get_file_at(files.root / "shader.json"));
    EXPECT_EQ(loader.diagnostics().publications, 0u);
    EXPECT_TRUE(loader.shader_manifests()->empty());
}

TEST(shader_assets, preserved_generation) {
    TemporaryShaders files;
    RecordingProvider provider;
    CE::Assets::Loading::Loader loader(files.root);
    loader.upload(files.prepare(), provider);
    const auto old = CE::Assets::Shaders::Manager::get().find_program("test:program");
    files.write("shaders/source.vert", "changed vertex");
    files.write("shader.json", changed(shader_json, "\"name\":\"images\"", "\"name\":\"second\""));
    loader.upload(files.prepare(), provider);
    EXPECT_EQ(provider.program_builds, 1u);
    const auto material = CE::Assets::Shaders::Manager::get().find_material("test:second");
    ASSERT_NE(material, nullptr);
    const auto* pipeline = dynamic_cast<const SnapshotPipeline*>(material->definition().pipeline.get());
    ASSERT_NE(pipeline, nullptr);
    EXPECT_EQ(pipeline->program, old->executable);
    EXPECT_EQ(pipeline->program->snapshot.sources[0].bytes, "original vertex");
}

TEST(shader_assets, reload_failure) {
    TemporaryShaders files;
    RecordingProvider provider;
    CE::Assets::Loading::Loader loader(files.root);
    loader.upload(files.prepare(), provider);
    const auto old = CE::Assets::Shaders::Manager::get().find_material("test:images");
    const auto metadata = loader.shader_manifests();
    files.write("shaders/source.vert", "changed vertex");
    provider.fail_program = true;
    EXPECT_THROW(loader.upload(files.prepare(), provider, true), CE::Exceptions::failed_operation);
    EXPECT_EQ(loader.diagnostics().replacements, 0u);
    provider.fail_program = false;
    provider.fail_material = "test:images";
    EXPECT_THROW(loader.upload(files.prepare(), provider, true), CE::Exceptions::failed_operation);
    EXPECT_EQ(loader.diagnostics().replacements, 1u);
    EXPECT_FALSE(loader.diagnostics().completed);
    EXPECT_EQ(loader.shader_manifests(), metadata);
    EXPECT_EQ(CE::Assets::Shaders::Manager::get().find_material("test:images"), old);
    provider.fail_material.clear();
    loader.upload(files.prepare(), provider, true);
    EXPECT_EQ(loader.diagnostics().replacements, 2u);
    EXPECT_NE(CE::Assets::Shaders::Manager::get().find_material("test:images"), old);
    const auto values = old->resolve({}, {}, {}, {{"image", CE::Rendering::Parameters::ImageBinding{std::make_shared<MemoryImage>(), 0}}});
    EXPECT_FLOAT_EQ(std::get<float>(values.at("weight")), 1.0f);
}

TEST(shader_assets, owner_thread) {
    TemporaryShaders files;
    RecordingProvider provider;
    CE::Assets::Loading::Loader loader(files.root);
    loader.upload(files.prepare(), provider);
    auto prepared = files.prepare();
    std::exception_ptr failure;
    std::thread foreign([&] {
        try {
            loader.upload(std::move(prepared), provider, true);
        } catch (...) {
            failure = std::current_exception();
        }
    });
    foreign.join();
    ASSERT_NE(failure, nullptr);
    EXPECT_THROW(std::rethrow_exception(failure), CE::Exceptions::failed_operation);
    EXPECT_EQ(provider.program_builds, 1u);
}

TEST(shader_assets, invalid_definitions) {
    for (const auto& document : {
        changed(shader_json, "\"version\":\"2.0\"", "\"version\":\"3.0\""),
        changed(shader_json, "\"fragment\":", "\"compute\":"),
        changed(shader_json, "\"name\":\"images\"", "\"name\":\"program\""),
        changed(
            shader_json, "\"key\":\"image\",\"type\":\"sampler2d\"", "\"key\":\"image\",\"type\":\"sampler2d\",\"default\":\"pixel.png\""
        ),
        changed(shader_json, "\"bindings\":", "\"defaults\":{\"image\":\"pixel.png\"},\"bindings\":"),
        changed(shader_json, "\"namespace\":\"test\"", "\"namespace\":\"test\",\"namespace\":\"other\"")}) {
        SCOPED_TRACE(document);
        std::istringstream input(document);
        EXPECT_THROW(
            static_cast<void>(CE::Assets::Manifests::Reader::parse_shader(input, "graphics/invalid.json")),
            CE::Exceptions::runtime_exception
        );
    }
}

TEST(shader_assets, preparation_failure) {
    TemporaryShaders files;
    RecordingProvider provider;
    CE::Assets::Loading::Loader loader(files.root);
    loader.upload(files.prepare(), provider);
    const auto old = CE::Assets::Shaders::Manager::get().find_program("test:program");
    const auto metadata = loader.shader_manifests();
    files.write("shader.json", changed(shader_json, "\"program\":\"test:program\"", "\"program\":\"test:images\""));
    EXPECT_THROW(static_cast<void>(files.prepare()), CE::Exceptions::runtime_exception);
    EXPECT_EQ(CE::Assets::Shaders::Manager::get().find_program("test:program"), old);
    EXPECT_EQ(loader.shader_manifests(), metadata);
    EXPECT_EQ(provider.program_builds, 1u);
    files.write("shader.json", shader_json);
    files.write("duplicate.json", shader_json);
    files.write("graphics-manifests.json", R"JSON({"version":"2.0","manifests":["shader.json","duplicate.json"]})JSON");
    EXPECT_THROW(static_cast<void>(files.prepare()), CE::Exceptions::runtime_exception);
    files.write("graphics-manifests.json", R"JSON({"version":"2.0","manifests":["shader.json"]})JSON");
    files.write("shader.json", changed(shader_json, "\"asset_class\":\"shader\"", "\"asset_class\":\"unknown\""));
    EXPECT_THROW(static_cast<void>(files.prepare()), CE::Exceptions::runtime_exception);
    files.write("shader.json", shader_json);
    std::filesystem::remove(files.root / "shaders/source.vert");
    EXPECT_THROW(static_cast<void>(files.prepare()), CE::Exceptions::runtime_exception);
}

TEST(shader_assets, asset_selection) {
    TemporaryShaders files;
    files.grid();
    RecordingProvider provider;
    CE::Assets::Loading::Loader loader(files.root);
    auto prepared = loader.prepare();
    ASSERT_EQ(prepared.shaders.size(), 1u);
    ASSERT_EQ(prepared.manifests.size(), 1u);
    loader.upload(std::move(prepared), provider);
    const auto sprite = CE::Assets::Sprites::Manager::get().get_asset("test:pixel");
    const auto tileset = CE::Assets::Tilesets::Manager::get().get_asset("test:soil");
    const auto selected = CE::Assets::Shaders::Manager::get().find_material("test:images");
    ASSERT_NE(sprite, nullptr);
    ASSERT_NE(tileset, nullptr);
    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(sprite->material, selected);
    EXPECT_EQ(tileset->tile(0).material, selected);
    CE::Rendering::Submission::Context2D context;
    context.image = CE::Rendering::Submission::ImageParameter2D{"image", 0};
    const auto packet = CE::Rendering::Submission::resolve_sprite(*sprite, 0, {}, context);
    EXPECT_EQ(packet.material, selected);
    EXPECT_EQ(std::get<CE::Rendering::Parameters::ImageBinding>(packet.parameters.at("image")).image, sprite->texture);
    EXPECT_EQ(CE::Rendering::Submission::resolve_tile(tileset->tile(0), {}, context).material, selected);
    CE::Assets::Tilesets::Clip clip;
    clip.frames = {{0, std::chrono::milliseconds{10}}};
    const CE::Assets::Tilesets::Animation animation(clip, tileset->geometry, tileset->texture, tileset->material);
    EXPECT_EQ(CE::Rendering::Submission::resolve_tile(animation, {}, context).material, selected);
    files.write("shader.json", changed(shader_json, "\"name\":\"images\"", "\"name\":\"second\""));
    files.write("grid.json", changed(grid_json, "\"shader\":\"test:images\"", "\"shader\":\"test:second\""));
    loader.upload(files.prepare(), provider);
    CE::Rendering::DrawStyle2D effect;
    effect.material = CE::Assets::Shaders::Manager::get().find_material("test:second");
    ASSERT_NE(effect.material, selected);
    EXPECT_EQ(CE::Rendering::Submission::resolve_sprite(*sprite, 0, effect, context).material, effect.material);
    EXPECT_EQ(sprite->material, selected);
}

TEST(shader_assets, asset_references) {
    TemporaryShaders files;
    files.grid();
    files.write("grid.json", changed(grid_json, "\"shader\":\"test:images\"", "\"shader\":\"test:program\""));
    EXPECT_THROW(static_cast<void>(files.prepare()), CE::Exceptions::runtime_exception);
    files.write("grid.json", grid_json);
    files.write("shader.json", changed(shader_json, "\"topology\":\"triangle_strip\"", "\"topology\":\"triangles\""));
    EXPECT_THROW(static_cast<void>(files.prepare()), CE::Exceptions::invalid_args);
    const auto legacy = changed(grid_json, "\"version\":\"2.0\"", "\"version\":\"1.0\"");
    std::istringstream input(legacy);
    EXPECT_THROW(static_cast<void>(CE::Assets::Manifests::Reader::parse(input, "graphics/legacy.json")), CE::Exceptions::runtime_exception);
    auto entry_selection = changed(grid_json, "\"pixel\":{\"grid\":", "\"pixel\":{\"shader\":\"fx:images\",\"grid\":");
    std::istringstream modern(entry_selection);
    const auto parsed = CE::Assets::Manifests::Reader::parse(modern, "graphics/modern.json");
    EXPECT_EQ(parsed.sprites.front().shader, "fx:images");
    EXPECT_EQ(parsed.tilesets.front().shader, "test:images");
}

TEST(shader_assets, sampling_policy) {
    TemporaryShaders files;
    RecordingProvider provider;
    CE::Assets::Loading::Loader loader(files.root);
    loader.upload(files.prepare(), provider);
    const auto engine_default = CE::Assets::Shaders::Manager::get().find_material("test:images");
    const CE::Rendering::Parameters::Set draw{{"image", CE::Rendering::Parameters::ImageBinding{std::make_shared<MemoryImage>(), 0}}};
    EXPECT_TRUE(engine_default->definition().sampling.empty());
    EXPECT_EQ(std::get<CE::Rendering::Parameters::ImageBinding>(engine_default->resolve({}, {}, {}, draw).at("image")).sampler, nullptr);
    EXPECT_EQ(CE::Rendering::Sampling::Options{}.anisotropy, CE::Rendering::Sampling::Anisotropy::MaximumSupported);
    files.write("shader.json", changed(shader_json, "\"bindings\":",
        "\"sampling\":{\"image\":{\"minification\":\"nearest\",\"magnification\":\"nearest\",\"mipmaps\":\"none\",\"anisotropy\":\"disabled\"}},\"bindings\":"));
    loader.upload(files.prepare(), provider, true);
    const auto effect = CE::Assets::Shaders::Manager::get().find_material("test:images");
    const auto resolved = effect->resolve({}, {}, {}, draw);
    const auto sampler = std::get<CE::Rendering::Parameters::ImageBinding>(resolved.at("image")).sampler;
    ASSERT_NE(sampler, nullptr);
    EXPECT_EQ(sampler->options().minification, CE::Rendering::Sampling::Filter::Nearest);
    EXPECT_EQ(sampler->options().anisotropy, CE::Rendering::Sampling::Anisotropy::Disabled);
    EXPECT_FALSE(effect->definition().defaults.contains("image"));
    auto explicit_draw = draw;
    const auto explicit_sampler = provider.create_sampler({});
    std::get<CE::Rendering::Parameters::ImageBinding>(explicit_draw.at("image")).sampler = explicit_sampler;
    EXPECT_EQ(
        std::get<CE::Rendering::Parameters::ImageBinding>(effect->resolve({}, {}, {}, explicit_draw).at("image")).sampler, explicit_sampler
    );
}

TEST(shader_assets, literal_types) {
    struct Literal {
        std::string type;
        std::string value;
        CE::Rendering::Parameters::Type expected;
    };
    for (const auto& literal : std::array<Literal, 8>{
             {{"float", "1.5", CE::Rendering::Parameters::Type::Float},
              {"int", "-2147483648", CE::Rendering::Parameters::Type::Int},
              {"uint", "4294967295", CE::Rendering::Parameters::Type::UInt},
              {"bool", "true", CE::Rendering::Parameters::Type::Bool},
              {"vec2", "[1,2]", CE::Rendering::Parameters::Type::Vec2},
              {"vec3", "[1,2,3]", CE::Rendering::Parameters::Type::Vec3},
              {"vec4", "[1,2,3,4]", CE::Rendering::Parameters::Type::Vec4},
              {"mat4", "[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]", CE::Rendering::Parameters::Type::Mat4}}
         }) {
        SCOPED_TRACE(literal.type);
        std::istringstream input(changed(shader_json, "\"type\":\"float\",\"default\":1.0",
            "\"type\":\"" + literal.type + "\",\"default\":" + literal.value));
        const auto manifest = CE::Assets::Manifests::Reader::parse_shader(input, "graphics/literals.json");
        const auto& value = *manifest.materials.front().parameters.back().default_value;
        EXPECT_EQ(CE::Rendering::Parameters::type_of(CE::Assets::Shaders::parameter_value(value)), literal.expected);
        if (literal.expected == CE::Rendering::Parameters::Type::Mat4)
            EXPECT_FLOAT_EQ(std::get<glm::mat4>(value)[1][2], 7.0f);
    }
    for (const auto& value : {"1e300", "[1,2]", "\"image.png\""}) {
        std::istringstream input(changed(shader_json, "\"default\":1.0", std::string("\"default\":") + value));
        EXPECT_THROW(
            static_cast<void>(CE::Assets::Manifests::Reader::parse_shader(input, "graphics/invalid-literal.json")),
            CE::Exceptions::runtime_exception
        );
    }
}

TEST(shader_assets, identity_kind) {
    TemporaryShaders files;
    RecordingProvider provider;
    CE::Assets::Loading::Loader loader(files.root);
    loader.upload(files.prepare(), provider);
    const auto retained = CE::Assets::Shaders::Manager::get().find_material("test:images");
    auto conflict = changed(shader_json, "\"name\":\"program\"", "\"name\":\"images\"");
    conflict = changed(conflict, "\"name\":\"images\",\"program\"", "\"name\":\"second\",\"program\"");
    conflict = changed(conflict, "\"program\":\"test:program\"", "\"program\":\"test:images\"");
    files.write("shader.json", conflict);
    EXPECT_THROW(loader.upload(files.prepare(), provider, true), CE::Exceptions::invalid_args);
    EXPECT_EQ(CE::Assets::Shaders::Manager::get().find_material("test:images"), retained);
    EXPECT_EQ(loader.diagnostics().replacements, 0u);
}

TEST(shader_assets, selected_program_reference) {
    TemporaryShaders files;
    const auto program_end = shader_json.find("\n      \"materials\"");
    ASSERT_NE(program_end, std::string::npos);
    auto program_only = shader_json.substr(0, program_end);
    program_only += "\"materials\":[]}";
    const auto program_begin = shader_json.find("\"programs\":[");
    auto material_only = shader_json;
    material_only.replace(program_begin, program_end - program_begin, "\"programs\":[],");
    files.write("program.json", program_only);
    files.write("shader.json", material_only);
    EXPECT_THROW(static_cast<void>(files.prepare()), CE::Exceptions::runtime_exception);
    files.write("graphics-manifests.json", R"JSON({"version":"2.0","manifests":["shader.json","program.json"]})JSON");
    RecordingProvider provider;
    CE::Assets::Loading::Loader(files.root).upload(files.prepare(), provider);
    EXPECT_EQ(provider.program_builds, 1u);
    EXPECT_EQ(provider.material_builds, 1u);
}
