#pragma once

#include <cheryl/export.h>

#include <assets/types/2d/base/asset2d.h>
#include <core/resources/memory/shared.h>
#include <assets/types/primitives/frame.h>
#include <assets/definitions/tileset.h>
#include <assets/selection/tile-selection.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <cstddef>

namespace CE::Assets::Tilesets {
/** Transfer bundle for an owned definition snapshot and retained uploaded resources; aggregate construction validates nothing. */
    struct Data {
        CE::Memory::Shared<CE::Rendering::Geometry::Buffer2D> geometry; ///< Uploaded row-major cell-strip geometry retained by the constructed asset.
        CE::Memory::Shared<CE::Rendering::Resources::Image> texture; ///< Uploaded atlas image retained by the constructed asset.
        Definition definition; ///< Owned CPU metadata copied/moved into the immutable asset snapshot.
        std::shared_ptr<const CE::Rendering::Resources::Material> material = {}; ///< Optional retained explicit/loader-selected material; direct construction creates no default.
    };

/** One static grid-cell offset retaining atlas resources; its inherited one-slot cursor does not select another grid cell. */
    struct Tile final : Content2D,
                        CE::Assets::Animation::Cursor<Tile> {
        explicit Tile(
            std::size_t cell,
            const CE::Memory::Shared<CE::Rendering::Geometry::Buffer2D>& geometry,
            const CE::Memory::Shared<CE::Rendering::Resources::Image>& texture,
            std::shared_ptr<const CE::Rendering::Resources::Material> material = {}
        )
        : Content2D(geometry, texture, std::move(material)), CE::Assets::Animation::Cursor<Tile>(cell, 0, 1) {}

        [[nodiscard]] std::size_t cell() const { return offset_; }
    };

    /** Owns a clip copy and retains its resources. CE::Assets::Animation::Cursor's index selects a clip entry,
     * not a grid cell; looping wraps and nonlooping clamps. An empty clip throws.
     * The caller schedules index changes from millisecond durations on one owner.
     */
    struct Animation final : Content2D,
                             CE::Assets::Animation::Cursor<Animation> {
    private:
        Clip definition_;

    public:
        CE_API explicit Animation(
            Clip definition,
            const CE::Memory::Shared<CE::Rendering::Geometry::Buffer2D>& geometry,
            const CE::Memory::Shared<CE::Rendering::Resources::Image>& texture,
            std::shared_ptr<const CE::Rendering::Resources::Material> material = {}
        );

        [[nodiscard]] const Clip& definition() const { return definition_; }
        [[nodiscard]] CE_API std::chrono::milliseconds frame_duration() const;
        [[nodiscard]] bool loops() const { return definition_.loop; }
    };

    /** Shared tile grid plus definitions for static cells, animated targets, and autotile rules.
     * Selection samples through the caller's callback and resolves simulation-owned time.
     * Direct construction rejects duplicate targets but does not revalidate all manifest
     * fields. Keep definitions/resources stable for concurrent read-only queries.
     */
    struct Asset final : Content2D {
    private:
        Definition definition_;
        std::unordered_map<CE::Assets::Grids::CellIndex, std::string> animation_targets_;

    public:
        CE_API explicit Asset(Data data);

        // Retained resource values; tile() rejects out-of-grid cells. animation() throws
        // out_of_range for an unknown name; animation_for() returns no value for no target.
        [[nodiscard]] CE_API Tile tile(std::size_t cell) const;
        [[nodiscard]] CE_API Animation animation(const std::string& name) const;
        [[nodiscard]] CE_API std::optional<Animation> animation_for(std::size_t target) const;
        // Resolve only the original target's clip; frame cells never recurse. Negative
        // time or invalid timelines throw invalid_args; original/final grid violations
        // throw bad_request. Static targets still require nonnegative time.
        [[nodiscard]] CE_API CE::Assets::Grids::CellIndex
        cell_at(CE::Assets::Grids::CellIndex target, std::chrono::milliseconds elapsed) const;
        // Unknown names throw out_of_range. Preserve selector failure values, otherwise
        // apply cell_at() once. No callback is retained; submit the returned cell value.
        [[nodiscard]] CE_API CE::Assets::Tilesets::Selection::Result select_tile(
            const std::string& name,
            const CE::Assets::Tilesets::Selection::Sampler& sampler,
            const CE::Assets::Tilesets::Selection::Options& options,
            std::chrono::milliseconds elapsed
        ) const;
        // References borrow this CE::Assets::Tilesets::Asset's lifetime; unknown metadata names throw out_of_range.
        [[nodiscard]] CE_API const CE::Assets::Grids::View& view(const std::string& name) const;
        [[nodiscard]] CE_API CE::Assets::Grids::CellIndex orientation(const std::string& name) const;
        [[nodiscard]] CE_API const Rule& autotile(const std::string& name) const;
        [[nodiscard]] const Definition& definition() const { return definition_; }
    };
}

/**
 * @fn CE::Assets::Tilesets::Tile::Tile(std::size_t cell, const CE::Memory::Shared<CE::Rendering::Geometry::Buffer2D>& geometry, const CE::Memory::Shared<CE::Rendering::Resources::Image>& texture, std::shared_ptr<const CE::Rendering::Resources::Material> material)
 * @brief Retain one static cell and resource handles without grid/domain validation.
 * @param cell Original grid-cell offset; direct construction does not check it.
 * @param geometry Retained uploaded atlas geometry.
 * @param texture Retained atlas image.
 * @param material Optional retained material; no default is constructed.
 * The inherited cursor has index0/limit1; set_frame() cannot change this cell offset.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn std::size_t CE::Assets::Tilesets::Tile::cell() const
 * @brief Read the retained original grid-cell offset, independent of inherited cursor ordinal.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn CE::Assets::Tilesets::Animation::Animation(Clip definition, const CE::Memory::Shared<CE::Rendering::Geometry::Buffer2D>& geometry, const CE::Memory::Shared<CE::Rendering::Resources::Image>& texture, std::shared_ptr<const CE::Rendering::Resources::Material> material)
 * @brief Own a clip copy and retain its uploaded resources.
 * @param definition Nonempty ordered timeline; loop selects inherited Wrap or Clamp policy.
 * @param geometry Retained atlas geometry.
 * @param texture Retained atlas image.
 * @param material Optional retained material, with no default construction.
 * An empty timeline rejects via Cursor; direct construction does not validate durations, cells or backend compatibility. Caller advances the ordinal from frame_duration() on one owner.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn const CE::Assets::Tilesets::Clip& CE::Assets::Tilesets::Animation::definition() const
 * @brief Borrow the owned clip copy for this animation handle's lifetime.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn std::chrono::milliseconds CE::Assets::Tilesets::Animation::frame_duration() const
 * @brief Read the selected clip-entry duration in milliseconds; does not advance time or validate positivity.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn bool CE::Assets::Tilesets::Animation::loops() const
 * @brief Read the clip's loop flag used to select inherited ordinal policy.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn CE::Assets::Tilesets::Asset::Asset(Data data)
 * @brief Own tileset metadata and retain uploaded resources.
 * @param data Resource/metadata transfer bundle.
 * @throws CE::Exceptions::invalid_args If multiple animations target the same original cell.
 * Does not revalidate every grid, clip duration/cell or backend domain, and creates no default material. Keep source resources valid for concurrent read-only use.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn CE::Assets::Tilesets::Tile CE::Assets::Tilesets::Asset::tile(std::size_t cell) const
 * @brief Create a retained resource view of a static grid cell.
 * @param cell Row-major ordinal inside this definition's grid.
 * @throws CE::Exceptions::bad_request For an out-of-grid cell. No animation substitution is performed.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn CE::Assets::Tilesets::Animation CE::Assets::Tilesets::Asset::animation(const std::string& name) const
 * @brief Copy a named clip into an independent ordinal cursor retaining resources.
 * @param name Exact clip name.
 * @throws std::out_of_range For a missing name. An empty clip also rejects.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn std::optional<CE::Assets::Tilesets::Animation> CE::Assets::Tilesets::Asset::animation_for(std::size_t target) const
 * @brief Look up a clip only for its original target cell.
 * @param target Original target ordinal; this lookup does not validate grid bounds.
 * @return Independent retained animation, or no value when no clip targets that ordinal.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn CE::Assets::Grids::CellIndex CE::Assets::Tilesets::Asset::cell_at(CE::Assets::Grids::CellIndex target, std::chrono::milliseconds elapsed) const
 * @brief Resolve at most one original target's animation from caller-owned time.
 * @param target Original row-major cell inside this grid.
 * @param elapsed Nonnegative simulation milliseconds, even for static targets.
 * @return Original static cell or selected clip-frame cell; never recursively animates the result.
 * Negative/invalid/overflowing timelines throw invalid_args; original/final grid violations throw bad_request.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn CE::Assets::Tilesets::Selection::Result CE::Assets::Tilesets::Asset::select_tile(const std::string& name, const CE::Assets::Tilesets::Selection::Sampler& sampler, const CE::Assets::Tilesets::Selection::Options& options, std::chrono::milliseconds elapsed) const
 * @brief Sample a named CPU autotile rule, then resolve successful target animation once.
 * @param name Exact autotile rule name; missing names throw out_of_range.
 * @param sampler Non-retained synchronous world callback.
 * @param options Neighborhood fallback and deterministic variant seed.
 * @param elapsed Nonnegative simulation milliseconds; invalid time rejects even if selection would fail.
 * @return Selector failure unchanged, or final grid cell from cell_at(). Sampler, metadata and timeline failures propagate.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn const CE::Assets::Tilesets::Rule& CE::Assets::Tilesets::Asset::autotile(const std::string& name) const
 * @brief Borrow a named rule for this asset's lifetime; unknown names throw out_of_range.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn const CE::Assets::Tilesets::Definition& CE::Assets::Tilesets::Asset::definition() const
 * @brief Borrow the owned stable metadata for this asset's lifetime.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn const CE::Assets::Grids::View& CE::Assets::Tilesets::Asset::view(const std::string& name) const
 * @brief Borrow a named view for this asset's lifetime.
 * @param name Exact metadata key; unknown names throw std::out_of_range. No geometry or cell selection is changed.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn CE::Assets::Grids::CellIndex CE::Assets::Tilesets::Asset::orientation(const std::string& name) const
 * @brief Copy a named static cell ordinal.
 * @param name Exact metadata key; unknown names throw std::out_of_range. No animation clock or rendering mutation occurs.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */
