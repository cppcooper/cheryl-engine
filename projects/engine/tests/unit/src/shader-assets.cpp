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
#error Shader asset loading must not depend on OpenGL.
#endif

namespace {
    using namespace CE::Assets;
    namespace fs = std::filesystem;

    const std::string shader_json = R"JSON({
      "asset_class":"shader","version":"1.0","namespace":"test",
      "programs":[{"name":"program","stages":{"vertex":"shaders/source.vert","fragment":"shaders/source.frag"}}],
      "materials":[{"name":"images","program":"test:program","vertex_layout":"position3_uv2","topology":"triangle_strip",
        "parameters":[{"key":"image","type":"sampler2d"},{"key":"weight","type":"float","default":1.0}],
        "bindings":{"recording":{"parameters":{"image":"image","weight":"weight"}}}}]
    })JSON";

    const std::string grid_json = R"JSON({
      "$schema":"./schemas/asset-manifest-1.1.schema.json","version":"1.1","namespace":"test",
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
        const fs::path root = fs::temp_directory_path() /
            ("cheryl-shaders-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + '-' +
             std::to_string(next.fetch_add(1)));

        TemporaryShaders() {
            write("graphics-manifests.json", R"JSON({"version":"1.0","manifests":["shader.json"]})JSON");
            write("shader.json", shader_json);
            write("shaders/source.vert", "original vertex");
            write("shaders/source.frag", "original fragment");
        }
        ~TemporaryShaders() {
            std::error_code error;
            fs::remove_all(root, error);
        }
        void write(const fs::path& relative, const std::string& bytes) const {
            fs::create_directories((root / relative).parent_path());
            std::ofstream(root / relative, std::ios::binary) << bytes;
        }
        [[nodiscard]] PreparedAssets prepare() const {
            return Loader(root).prepare_graphics({"graphics-manifests.json"});
        }
        void grid() const {
            fs::copy_file(
                fs::path(CHERYL_SOURCE_DIR) / "projects/modules/graphics/opengl/tests/fixtures/rgba-two-rows.png", root / "pixel.png"
            );
            write("grid.json", grid_json);
            write("graphics-manifests.json", R"JSON({"version":"1.0","manifests":["grid.json","shader.json","shader.json"]})JSON");
        }
    };

    struct SnapshotShader final : Shader {
        const ShaderProgramRecipe snapshot;

        explicit SnapshotShader(ShaderProgramRecipe recipe) : snapshot(std::move(recipe)) {}
        void bind_pass(const ShaderPass&) override {}
        void bind_draw(const ShaderDraw&) override {}
        void use() override {}
        void set_uniform_value(const char*, float) override {}
        void set_uniform_value(const char*, int) override {}
        void set_uniform_value(const char*, unsigned int) override {}
        void set_uniform_value(const char*, bool) override {}
        void set_uniform_matrix(const char*, const glm::mat4&) override {}
    };

    struct SnapshotPipeline final : Pipeline {
        const std::shared_ptr<SnapshotShader> program;

        SnapshotPipeline(PipelineDefinition definition, std::shared_ptr<SnapshotShader> executable)
        : Pipeline(std::move(definition)), program(std::move(executable)) {}
    };

    struct MemoryImage final : Image {
        const PixelSize size;

        explicit MemoryImage(PixelSize dimensions = {1, 1}) : size(dimensions) {}
        [[nodiscard]] PixelSize pixel_size() const override { return size; }
        void bind(std::uint32_t) const override {}
    };

    struct MemoryGeometry final : Geometry2D {
        const std::size_t count;
        const PrimitiveTopology primitive;

        MemoryGeometry(std::size_t count, PrimitiveTopology primitive) : count(count), primitive(primitive) {}
        [[nodiscard]] VertexLayout2D vertex_layout() const noexcept override { return VertexLayout2D::Position3UV2; }
        [[nodiscard]] PrimitiveTopology topology() const noexcept override { return primitive; }
        [[nodiscard]] std::size_t vertex_count() const noexcept override { return count; }
        void bind() const override {}
        void draw(std::size_t, std::size_t) const override {}
    };

    struct MemorySampler final : Sampler {
        explicit MemorySampler(SamplerOptions options) : Sampler(options, 1) {}
        void bind(std::uint32_t) const override {}
    };

    struct RecordingProvider final : ResourceProvider, ShaderAssetBuilder {
        bool supported = true;
        bool fail_program = false;
        std::string fail_material;
        std::size_t program_builds = 0;
        std::size_t material_builds = 0;
        std::size_t image_uploads = 0;

        [[nodiscard]] ShaderAssetBuilder* shader_asset_builder() noexcept override { return supported ? this : nullptr; }
        void validate_program(const ShaderProgramRecipe& recipe) const override { validate_shader_program(recipe); }
        void validate_material(const ShaderMaterialRecipe& recipe, const ShaderProgramRecipe& program) const override {
            validate_shader_material(recipe, program);
        }
        [[nodiscard]] std::shared_ptr<Shader> build_program(const ShaderProgramRecipe& recipe) override {
            if (fail_program)
                throw CE::Exceptions::failed_operation(CE_HERE, "Simulated program failure");
            ++program_builds;
            return std::make_shared<SnapshotShader>(recipe);
        }
        [[nodiscard]] std::shared_ptr<const Material> build_material(
            const ShaderMaterialRecipe& recipe, const ShaderProgramRecipe& program, const std::shared_ptr<Shader>& executable
        ) override {
            if (recipe.id == fail_material)
                throw CE::Exceptions::failed_operation(CE_HERE, "Simulated material failure");
            auto snapshot = std::dynamic_pointer_cast<SnapshotShader>(executable);
            if (!snapshot || snapshot->snapshot.sources[0].bytes != program.sources[0].bytes)
                throw CE::Exceptions::failed_operation(CE_HERE, "Program metadata does not describe its executable");
            ++material_builds;
            MaterialDefinition definition;
            definition.pipeline = std::make_shared<SnapshotPipeline>(shader_pipeline_definition(recipe, program), snapshot);
            definition.defaults = shader_material_defaults(recipe);
            for (const auto& [key, options] : recipe.sampling)
                definition.sampling.emplace(key, create_sampler(options));
            return std::make_shared<const Material>(std::move(definition));
        }
        [[nodiscard]] std::shared_ptr<Image> create_image(const DecodedImage& image) override {
            ++image_uploads;
            return std::make_shared<MemoryImage>(image.size);
        }
        [[nodiscard]] std::shared_ptr<const Sampler> create_sampler(const SamplerOptions& options) override {
            return std::make_shared<MemorySampler>(options);
        }
        [[nodiscard]] std::shared_ptr<Image> create_font_atlas(std::span<const unsigned char>, PixelSize size) override {
            return std::make_shared<MemoryImage>(size);
        }
        [[nodiscard]] std::shared_ptr<Geometry2D> upload_geometry(
            std::span<const CE::Vertex2D> vertices, PrimitiveTopology topology
        ) override {
            return std::make_shared<MemoryGeometry>(vertices.size(), topology);
        }
        [[nodiscard]] std::shared_ptr<Shader> link_program(const std::vector<fs::path>&) override {
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
    EXPECT_THROW(static_cast<void>(Loader(files.root).prepare()), CE::Exceptions::runtime_exception);
    files.write("shaders/source.vert", "changed vertex");
    fs::remove(files.root / "shaders/source.frag");
    RecordingProvider provider;
    Loader(files.root).upload(std::move(prepared), provider);
    const auto program = ShaderAssetMgr::get().get_program("test:program");
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
    Loader loader(files.root);
    EXPECT_THROW(loader.upload(prepared, provider), CE::Exceptions::failed_operation);
    EXPECT_FALSE(ShaderAssetMgr::get().contains("test:program"));
    EXPECT_FALSE(FileRegistry::get().get_file_at(files.root / "shader.json"));
    EXPECT_EQ(loader.diagnostics().publications, 0u);
    EXPECT_TRUE(loader.shader_manifests()->empty());
}

TEST(shader_assets, preserved_generation) {
    TemporaryShaders files;
    RecordingProvider provider;
    Loader loader(files.root);
    loader.upload(files.prepare(), provider);
    const auto old = ShaderAssetMgr::get().get_program("test:program");
    files.write("shaders/source.vert", "changed vertex");
    files.write("shader.json", changed(shader_json, "\"name\":\"images\"", "\"name\":\"second\""));
    loader.upload(files.prepare(), provider);
    EXPECT_EQ(provider.program_builds, 1u);
    const auto material = ShaderAssetMgr::get().get_material("test:second");
    ASSERT_NE(material, nullptr);
    const auto* pipeline = dynamic_cast<const SnapshotPipeline*>(material->definition().pipeline.get());
    ASSERT_NE(pipeline, nullptr);
    EXPECT_EQ(pipeline->program, old->executable);
    EXPECT_EQ(pipeline->program->snapshot.sources[0].bytes, "original vertex");
}

TEST(shader_assets, reload_failure) {
    TemporaryShaders files;
    RecordingProvider provider;
    Loader loader(files.root);
    loader.upload(files.prepare(), provider);
    const auto old = ShaderAssetMgr::get().get_material("test:images");
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
    EXPECT_EQ(ShaderAssetMgr::get().get_material("test:images"), old);
    provider.fail_material.clear();
    loader.upload(files.prepare(), provider, true);
    EXPECT_EQ(loader.diagnostics().replacements, 2u);
    EXPECT_NE(ShaderAssetMgr::get().get_material("test:images"), old);
    const auto values = old->resolve({}, {}, {}, {{"image", ImageBinding{std::make_shared<MemoryImage>(), 0}}});
    EXPECT_FLOAT_EQ(std::get<float>(values.at("weight")), 1.0f);
}

TEST(shader_assets, owner_thread) {
    TemporaryShaders files;
    RecordingProvider provider;
    Loader loader(files.root);
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
        changed(shader_json, "\"version\":\"1.0\"", "\"version\":\"2.0\""),
        changed(shader_json, "\"fragment\":", "\"compute\":"),
        changed(shader_json, "\"name\":\"images\"", "\"name\":\"program\""),
        changed(
            shader_json, "\"key\":\"image\",\"type\":\"sampler2d\"", "\"key\":\"image\",\"type\":\"sampler2d\",\"default\":\"pixel.png\""
        ),
        changed(shader_json, "\"bindings\":", "\"defaults\":{\"image\":\"pixel.png\"},\"bindings\":"),
        changed(shader_json, "\"namespace\":\"test\"", "\"namespace\":\"test\",\"namespace\":\"other\"")}) {
        SCOPED_TRACE(document);
        std::istringstream input(document);
        EXPECT_THROW(static_cast<void>(ManifestLoader::parse_shader(input, "invalid.json")), CE::Exceptions::runtime_exception);
    }
}

TEST(shader_assets, preparation_failure) {
    TemporaryShaders files;
    RecordingProvider provider;
    Loader loader(files.root);
    loader.upload(files.prepare(), provider);
    const auto old = ShaderAssetMgr::get().get_program("test:program");
    const auto metadata = loader.shader_manifests();
    files.write("shader.json", changed(shader_json, "\"program\":\"test:program\"", "\"program\":\"test:images\""));
    EXPECT_THROW(static_cast<void>(files.prepare()), CE::Exceptions::runtime_exception);
    EXPECT_EQ(ShaderAssetMgr::get().get_program("test:program"), old);
    EXPECT_EQ(loader.shader_manifests(), metadata);
    EXPECT_EQ(provider.program_builds, 1u);
    files.write("shader.json", shader_json);
    files.write("duplicate.json", shader_json);
    files.write("graphics-manifests.json", R"JSON({"version":"1.0","manifests":["shader.json","duplicate.json"]})JSON");
    EXPECT_THROW(static_cast<void>(files.prepare()), CE::Exceptions::runtime_exception);
    files.write("graphics-manifests.json", R"JSON({"version":"1.0","manifests":["shader.json"]})JSON");
    files.write("shader.json", changed(shader_json, "\"asset_class\":\"shader\"", "\"asset_class\":\"unknown\""));
    EXPECT_THROW(static_cast<void>(files.prepare()), CE::Exceptions::runtime_exception);
    files.write("shader.json", shader_json);
    fs::remove(files.root / "shaders/source.vert");
    EXPECT_THROW(static_cast<void>(files.prepare()), CE::Exceptions::runtime_exception);
}

TEST(shader_assets, asset_selection) {
    TemporaryShaders files;
    files.grid();
    RecordingProvider provider;
    Loader loader(files.root);
    auto prepared = loader.prepare();
    ASSERT_EQ(prepared.shaders.size(), 1u);
    ASSERT_EQ(prepared.manifests.size(), 1u);
    loader.upload(std::move(prepared), provider);
    const auto sprite = SpriteMgr::get().get_asset("test:pixel");
    const auto tileset = TilesetMgr::get().get_asset("test:soil");
    const auto selected = ShaderAssetMgr::get().get_material("test:images");
    ASSERT_NE(sprite, nullptr);
    ASSERT_NE(tileset, nullptr);
    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(sprite->material, selected);
    EXPECT_EQ(tileset->tile(0).material, selected);
    SubmissionContext2D context;
    context.image = ImageParameter2D{"image", 0};
    const auto packet = resolve_sprite(*sprite, 0, {}, context);
    EXPECT_EQ(packet.material, selected);
    EXPECT_EQ(std::get<ImageBinding>(packet.parameters.at("image")).image, sprite->texture);
    EXPECT_EQ(resolve_tile(tileset->tile(0), {}, context).material, selected);
    TileAnimationDefinition clip;
    clip.frames = {{0, std::chrono::milliseconds{10}}};
    const TileAnimation animation(clip, tileset->geometry, tileset->texture, tileset->material);
    EXPECT_EQ(resolve_tile(animation, {}, context).material, selected);
    files.write("shader.json", changed(shader_json, "\"name\":\"images\"", "\"name\":\"second\""));
    loader.upload(files.prepare(), provider);
    CE::RenderAPIs::DrawStyle2D effect;
    effect.material = ShaderAssetMgr::get().get_material("test:second");
    ASSERT_NE(effect.material, selected);
    EXPECT_EQ(resolve_sprite(*sprite, 0, effect, context).material, effect.material);
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
    auto legacy = changed(grid_json, "asset-manifest-1.1", "asset-manifest-1.0");
    legacy = changed(legacy, "\"version\":\"1.1\"", "\"version\":\"1.0\"");
    std::istringstream input(legacy);
    EXPECT_THROW(static_cast<void>(ManifestLoader::parse(input, "legacy.json")), CE::Exceptions::runtime_exception);
    auto entry_selection = changed(grid_json, "\"pixel\":{\"grid\":", "\"pixel\":{\"shader\":\"fx:images\",\"grid\":");
    std::istringstream modern(entry_selection);
    const auto parsed = ManifestLoader::parse(modern, "modern.json");
    EXPECT_EQ(parsed.sprites.front().shader, "fx:images");
    EXPECT_EQ(parsed.tilesets.front().shader, "test:images");
}

TEST(shader_assets, sampling_policy) {
    TemporaryShaders files;
    RecordingProvider provider;
    Loader loader(files.root);
    loader.upload(files.prepare(), provider);
    const auto engine_default = ShaderAssetMgr::get().get_material("test:images");
    const ParameterSet draw{{"image", ImageBinding{std::make_shared<MemoryImage>(), 0}}};
    EXPECT_TRUE(engine_default->definition().sampling.empty());
    EXPECT_EQ(std::get<ImageBinding>(engine_default->resolve({}, {}, {}, draw).at("image")).sampler, nullptr);
    EXPECT_EQ(SamplerOptions{}.anisotropy, ImageAnisotropy::MaximumSupported);
    files.write("shader.json", changed(shader_json, "\"bindings\":",
        "\"sampling\":{\"image\":{\"minification\":\"nearest\",\"magnification\":\"nearest\",\"mipmaps\":\"none\",\"anisotropy\":\"disabled\"}},\"bindings\":"));
    loader.upload(files.prepare(), provider, true);
    const auto effect = ShaderAssetMgr::get().get_material("test:images");
    const auto resolved = effect->resolve({}, {}, {}, draw);
    const auto sampler = std::get<ImageBinding>(resolved.at("image")).sampler;
    ASSERT_NE(sampler, nullptr);
    EXPECT_EQ(sampler->options().minification, ImageFilter::Nearest);
    EXPECT_EQ(sampler->options().anisotropy, ImageAnisotropy::Disabled);
    EXPECT_FALSE(effect->definition().defaults.contains("image"));
    auto explicit_draw = draw;
    const auto explicit_sampler = provider.create_sampler({});
    std::get<ImageBinding>(explicit_draw.at("image")).sampler = explicit_sampler;
    EXPECT_EQ(std::get<ImageBinding>(effect->resolve({}, {}, {}, explicit_draw).at("image")).sampler, explicit_sampler);
}

TEST(shader_assets, literal_types) {
    struct Literal {
        std::string type;
        std::string value;
        ParameterType expected;
    };
    for (const auto& literal : std::array<Literal, 8>{{
        {"float", "1.5", ParameterType::Float}, {"int", "-2147483648", ParameterType::Int},
        {"uint", "4294967295", ParameterType::UInt}, {"bool", "true", ParameterType::Bool},
        {"vec2", "[1,2]", ParameterType::Vec2}, {"vec3", "[1,2,3]", ParameterType::Vec3},
        {"vec4", "[1,2,3,4]", ParameterType::Vec4},
        {"mat4", "[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]", ParameterType::Mat4}}}) {
        SCOPED_TRACE(literal.type);
        std::istringstream input(changed(shader_json, "\"type\":\"float\",\"default\":1.0",
            "\"type\":\"" + literal.type + "\",\"default\":" + literal.value));
        const auto manifest = ManifestLoader::parse_shader(input, "literals.json");
        const auto& value = *manifest.materials.front().parameters.back().default_value;
        EXPECT_EQ(parameter_type(shader_parameter_value(value)), literal.expected);
        if (literal.expected == ParameterType::Mat4)
            EXPECT_FLOAT_EQ(std::get<glm::mat4>(value)[1][2], 7.0f);
    }
    for (const auto& value : {"1e300", "[1,2]", "\"image.png\""}) {
        std::istringstream input(changed(shader_json, "\"default\":1.0", std::string("\"default\":") + value));
        EXPECT_THROW(static_cast<void>(ManifestLoader::parse_shader(input, "invalid-literal.json")), CE::Exceptions::runtime_exception);
    }
}

TEST(shader_assets, identity_kind) {
    TemporaryShaders files;
    RecordingProvider provider;
    Loader loader(files.root);
    loader.upload(files.prepare(), provider);
    const auto retained = ShaderAssetMgr::get().get_material("test:images");
    auto conflict = changed(shader_json, "\"name\":\"program\"", "\"name\":\"images\"");
    conflict = changed(conflict, "\"name\":\"images\",\"program\"", "\"name\":\"second\",\"program\"");
    conflict = changed(conflict, "\"program\":\"test:program\"", "\"program\":\"test:images\"");
    files.write("shader.json", conflict);
    EXPECT_THROW(loader.upload(files.prepare(), provider, true), CE::Exceptions::invalid_args);
    EXPECT_EQ(ShaderAssetMgr::get().get_material("test:images"), retained);
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
    files.write("graphics-manifests.json", R"JSON({"version":"1.0","manifests":["shader.json","program.json"]})JSON");
    RecordingProvider provider;
    Loader(files.root).upload(files.prepare(), provider);
    EXPECT_EQ(provider.program_builds, 1u);
    EXPECT_EQ(provider.material_builds, 1u);
}
