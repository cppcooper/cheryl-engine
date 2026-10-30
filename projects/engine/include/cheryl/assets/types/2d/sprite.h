#pragma once

#include <cheryl/export.h>

#include <assets/types/2d/base/asset2d.h>
#include <core/resources/memory/shared.h>
#include <assets/definitions/sprite.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace CE::Assets::Sprites {
/** Transfer bundle for an owned definition snapshot and retained uploaded resources; aggregate construction validates nothing. */
    struct Data {
        CE::Memory::Shared<CE::Rendering::Geometry::Buffer2D> geometry; ///< Uploaded row-major cell-strip geometry retained by the constructed asset.
        CE::Memory::Shared<CE::Rendering::Resources::Image> texture; ///< Uploaded atlas image retained by the constructed asset.
        Definition definition; ///< Owned CPU metadata copied/moved into the immutable asset snapshot.
        std::shared_ptr<const CE::Rendering::Resources::Material> material = {}; ///< Optional retained explicit/loader-selected material; direct construction creates no default.
    };

    /** Independent cursor retaining shared immutable clip metadata, even after its
     * CE::Assets::Sprites::Asset is released. Copies copy playback state without retaining GPU handles.
     * Advance on its simulation owner, then publish cell(); mutation is unsynchronized.
     */
    class Animation final {
    public:
        CE_API Animation& operator[](std::size_t frame);
        // Wrap looping clips, clamp nonlooping clips, and reset time within the frame.
        CE_API void set_frame(std::size_t frame);
        // Finite nonnegative seconds; invalid time throws without changing the cursor.
        // Exact duration boundaries advance; nonlooping clips stop at their last frame.
        CE_API void advance(std::chrono::duration<double> elapsed);
        [[nodiscard]] std::size_t index() const { return index_; }
        [[nodiscard]] CE_API CE::Assets::Grids::CellIndex cell() const;
        [[nodiscard]] const Clip& definition() const { return *definition_; }
        [[nodiscard]] CE_API std::chrono::milliseconds frame_duration() const;
        [[nodiscard]] bool loops() const { return definition_->loop; }

    private:
        friend struct Asset;
        CE_API explicit Animation(std::shared_ptr<const Clip> definition);

        std::shared_ptr<const Clip> definition_;
        std::size_t index_ = 0;
        std::chrono::duration<double> elapsed_{};
        std::chrono::duration<double> cycle_duration_{};
    };

    /** Retains a definition snapshot and grid resources. Construction checks nonempty
     * cells/clips, frame cells/durations and duplicate clip keys, not all manifest
     * fields or backend compatibility. Read concurrently only while inputs are stable.
     */
    struct Asset final : Content2D {
        CE_API explicit Asset(Data data);

        CE_API Animation operator[](const std::string& animation) const;
        // Prefer an exact name/facing. Without a facing, one matching clip is allowed;
        // missing/ambiguous requests throw. operator[] uses this faceless lookup.
        CE_API Animation animation(const std::string& animation, std::optional<std::string> facing = std::nullopt) const;
        [[nodiscard]] CE_API bool has_animation(const std::string& animation, std::optional<std::string> facing = std::nullopt) const;
        // Metadata references borrow this CE::Assets::Sprites::Asset's lifetime; missing names throw out_of_range.
        [[nodiscard]] CE_API const CE::Assets::Grids::View& view(const std::string& name) const;
        [[nodiscard]] CE_API CE::Assets::Grids::CellIndex orientation(const std::string& name) const;
        [[nodiscard]] const Definition& definition() const { return *definition_; }

    private:
        [[nodiscard]] CE_API static std::string animation_key(const std::string& animation, const std::optional<std::string>& facing);

        std::shared_ptr<const Definition> definition_;
        std::unordered_map<std::string, std::size_t> animation_indices_;
    };
}

/**
 * @fn CE::Assets::Sprites::Animation& CE::Assets::Sprites::Animation::operator[](std::size_t frame)
 * @brief Select a clip-frame ordinal via set_frame(), resetting within-frame time.
 * @return This mutable cursor, not a new sprite or grid-cell view.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn void CE::Assets::Sprites::Animation::set_frame(std::size_t frame)
 * @brief Select a clip ordinal and reset elapsed time inside it.
 * @param frame Ordinal; looping clips wrap, nonlooping clips clamp to their last frame.
 * Mutation is unsynchronized and belongs to the simulation owner.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn void CE::Assets::Sprites::Animation::advance(std::chrono::duration<double> elapsed)
 * @brief Advance this independent cursor using simulation seconds.
 * @param elapsed Finite nonnegative duration; invalid values throw invalid_args before changing playback.
 * Exact duration boundaries advance to the next frame; loops wrap efficiently and nonlooping clips hold the final frame. This does not update a shared asset, GPU handle or wall clock.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn std::size_t CE::Assets::Sprites::Animation::index() const
 * @brief Read the current clip-frame ordinal, distinct from the selected grid cell.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn CE::Assets::Grids::CellIndex CE::Assets::Sprites::Animation::cell() const
 * @brief Read the current clip frame's row-major grid cell for draw submission.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn const CE::Assets::Sprites::Clip& CE::Assets::Sprites::Animation::definition() const
 * @brief Borrow immutable clip metadata retained by this cursor.
 * The cursor keeps the CPU snapshot alive even after its source Asset is released; no GPU handles are retained by the cursor.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn std::chrono::milliseconds CE::Assets::Sprites::Animation::frame_duration() const
 * @brief Read the selected frame's positive duration in milliseconds; this does not advance playback.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn bool CE::Assets::Sprites::Animation::loops() const
 * @brief Read whether this clip repeats rather than holding its last frame.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn CE::Assets::Sprites::Asset::Asset(Data data)
 * @brief Retain resources and own an immutable sprite definition snapshot.
 * @param data Resource/metadata transfer bundle; backend compatibility is the caller's contract.
 * Checks nonempty cell count, nonempty clips, positive durations, frame-cell bounds and duplicate name/facing keys. Invalid clip data rejects; it does not revalidate every manifest field or create default materials.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn CE::Assets::Sprites::Animation CE::Assets::Sprites::Asset::operator[](const std::string& animation) const
 * @brief Create an independent cursor using faceless animation lookup.
 * @param animation Exact clip name.
 * @return CPU-only playback cursor; same missing/ambiguous errors as animation().
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn CE::Assets::Sprites::Animation CE::Assets::Sprites::Asset::animation(const std::string& animation, std::optional<std::string> facing) const
 * @brief Create independent playback retaining the matching CPU clip snapshot.
 * @param animation Exact clip name.
 * @param facing Optional exact facing; when absent, an exact unqualified clip wins, otherwise one uniquely named facing clip is accepted.
 * @return Cursor initially at its first frame, independent of other entities.
 * @throws CE::Exceptions::bad_request For missing or ambiguous selection.
 * Stable asset reads may be concurrent; mutate returned cursors on their own owners.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn bool CE::Assets::Sprites::Asset::has_animation(const std::string& animation, std::optional<std::string> facing) const
 * @brief Check whether animation() has an exact or uniquely resolvable clip.
 * @return False for missing or ambiguous faceless queries; does not construct a cursor.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn const CE::Assets::Sprites::Definition& CE::Assets::Sprites::Asset::definition() const
 * @brief Borrow this asset's immutable metadata snapshot; reference lifetime follows this asset.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn const CE::Assets::Grids::View& CE::Assets::Sprites::Asset::view(const std::string& name) const
 * @brief Borrow a named view for this asset's lifetime.
 * @param name Exact metadata key; unknown names throw std::out_of_range. No geometry or cell selection is changed.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn CE::Assets::Grids::CellIndex CE::Assets::Sprites::Asset::orientation(const std::string& name) const
 * @brief Copy a named static cell ordinal.
 * @param name Exact metadata key; unknown names throw std::out_of_range. No animation clock or rendering mutation occurs.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */
