#pragma once

#include <cheryl/export.h>

#include <assets/resources/geometry2d.h>
#include <assets/resources/parameters.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace CE::Rendering::Resources {
    class Pipeline;
}

namespace CE::Rendering::Pipelines {
    /** Fixed blend intent; fragment RGB must follow the selected alpha convention. */
    enum class Blend {
        Opaque, ///< Replace destination color without blending.
        StraightAlpha, ///< Source RGB is unmultiplied; native blending applies source alpha.
        PremultipliedAlpha, ///< Source RGB already includes alpha; native blending uses it directly.
        Additive ///< Add source color using the backend's additive blend mapping.
    };

    /** Depth comparison policy; it is independent of camera dimensionality. */
    enum class Depth {
        Disabled, ///< No depth comparison; common validation also forbids depth writes.
        Less, ///< Pass fragments nearer than stored depth.
        LessEqual ///< Pass fragments nearer than or equal to stored depth.
    };

    // Front-facing triangles use counterclockwise winding.
    /** Triangle culling with counterclockwise front faces. */
    enum class Cull { None, ///< Draw both face orientations.
        Front, ///< Discard front-facing triangles.
        Back ///< Discard back-facing triangles.
    };

    /** Fixed per-pipeline intent, validated before publication. */
    struct State2D {
        Blend blend = Blend::StraightAlpha; ///< Color composition policy.
        Depth depth = Depth::Disabled; ///< Depth comparison; disabled by default.
        bool depth_write = false; ///< Whether successful fragments replace depth; requires enabled comparison.
        Cull cull = Cull::None; ///< Triangle face rejection policy.
    };

    // A pass constrains pipeline intent; it never overrides one draw's state.
    // Default 2D passes require disabled depth. Reset depth to allow either mode.
    /** Optional pass requirements checked against each pipeline; absent values permit either choice. */
    struct PassConstraints2D {
        std::optional<Blend> blend; ///< Required blend, or no restriction.
        std::optional<Depth> depth = Depth::Disabled; ///< Disabled-depth requirement by default; reset to allow either mode.
        std::optional<bool> depth_write; ///< Required depth-write flag, or no restriction.
        std::optional<Cull> cull; ///< Required face policy, or no restriction.
    };

    // Owned CPU recipe; source paths are not read by common validation. Publication
    // copies a snapshot into Pipeline; native builders additionally link/reflect it.
    /** Owned pipeline recipe; common validation does not open source files or reflect a program.
     * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
     */
    struct Definition {
        std::vector<std::filesystem::path> program_sources; ///< Nonempty stage paths interpreted by the selected backend builder.
        CE::Rendering::Geometry::Layout2D vertex_layout = CE::Rendering::Geometry::Layout2D::Position3UV2; ///< Exact uploaded vertex layout required for drawing.
        CE::Rendering::Geometry::Topology topology = CE::Rendering::Geometry::Topology::Triangles; ///< Exact uploaded primitive topology required for drawing.
        State2D state; ///< Fixed draw-state snapshot.
        CE::Rendering::Parameters::Contract parameters; ///< Public parameter keys, types and engine-semantic sources.
    };
}

namespace CE::Rendering::Materials {
    // All presets disable depth/culling. Color presets use colored triangles and
    // projection only; Textured/TexturedStrip use uncolored geometry and the
    // projection/view/model/scale/alpha semantics. Textured presets require the
    // custom Sampler2D "image" and preserve its default sampling. TexturedColor
    // multiplies sampled RGBA by vertex RGBA without changing alpha convention.
    /** Backend-owned material profiles, requested through build_builtin_material(). */
    enum class Builtin2D {
        SolidColor, ///< Colored triangles, projection only; no image parameter.
        TexturedColor, ///< Colored textured triangles, projection only, RGBA multiplication.
        Textured, ///< Uncolored textured triangles with projection/view/model/scale/alpha semantics.
        TexturedStrip ///< Same ordinary textured semantics for independent atlas-cell strips.
    };

    /** Immutable material recipe retaining a pipeline generation and copied custom defaults. */
    // Retains a pipeline generation; defaults own image handles/unit requests.
    // Material construction validates this partial custom layer, not a complete draw.
    struct Definition {
        std::shared_ptr<const CE::Rendering::Resources::Pipeline> pipeline; ///< Required retained immutable pipeline generation.
        CE::Rendering::Parameters::Set defaults; ///< Partial custom layer, above pass values and below per-draw values.
        std::map<std::string, std::shared_ptr<const CE::Rendering::Sampling::Sampler>, std::less<>> sampling = {}; ///< Sampler-key overrides used only when the resolved image binding has none.
    };
}

namespace CE::Rendering::Pipelines {
    // CPU-only structural validation; throws on empty paths, unsupported layout/
    // topology/state, disabled-depth writes or an invalid parameter contract.
    CE_API void validate(const Definition& definition);
}

/**
 * @fn void CE::Rendering::Pipelines::validate(const Definition& definition)
 * @brief Validate common CPU pipeline structure without opening stages or using a backend.
 * @param definition Stable owned recipe.
 * @throws CE::Exceptions::invalid_args For empty sources, unsupported layout/topology/state, disabled-depth writes or an invalid parameter contract.
 * Does not validate shader contents, native reflection, resource domains or pass compatibility. Inputs remain unchanged.
 */
