#pragma once

#include <cheryl/export.h>
#include <cheryl/text/font-collection.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace CE::Text {
    /** Requested numeric OpenType weight and italic state; nearest available faces are selected, without synthesizing a style. */
    struct FontStyle {
        std::uint16_t weight = 400; ///< Requested weight: 400 is Regular, 700 is Bold; the closest available weight is used.
        bool italic = false; ///< Request italic when true; available italic state is preferred before weight distance.
    };

    /** Borrowed face data; retain its Font while using these views. Family zero
     * is the selected family; later indices follow the configured fallback order.
     */
    struct FontFace {
        std::span<const unsigned char> bytes; ///< Borrowed immutable font-file bytes; retain this face's Font snapshot.
        std::uint32_t face_index{}; ///< Zero-based face within the retained font file or collection.
        FontStyle style; ///< Declared face weight and italic state.
        bool fallback = false; ///< Whether this face belongs to the retained fallback chain.
        std::string family; ///< Owned native family name, distinct from the application reference.
        std::size_t family_index{}; ///< Zero identifies the selected family; later values identify fallback families.
        bool builtin = false; ///< Whether this is the engine's embedded safety face.
    };

    /** Resolved immutable CPU font data, shared by copies and independent of
     * Context/toolkit lifetime. Save reference(), then restore through FontService;
     * the runtime handle and font bytes are not serialized.
     */
    class Font final {
        struct State;
        std::shared_ptr<const State> state_;
        CE_API explicit Font(std::shared_ptr<const State> state);
        friend class FontService;

    public:
        Font() = default;
        [[nodiscard]] bool operator==(const Font&) const noexcept = default;
        [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(state_); }
        [[nodiscard]] CE_API const std::string& reference() const;
        [[nodiscard]] CE_API std::span<const FontFace> faces() const;
        [[nodiscard]] CE_API FontCollection collection(FontStyle style = {}) const;
    };
}

/** @fn CE::Text::Font::Font()
 * Construct an empty handle. Test it before querying reference(), faces() or collection().
 */

/** @fn bool CE::Text::Font::operator==(const Font&) const noexcept
 * Compare retained snapshot identity, including empty handles. Equal reference strings alone do not imply equal snapshots.
 */

/** @fn CE::Text::Font::operator bool() const noexcept
 * Return whether a snapshot is retained; default and moved-from handles are empty.
 */

/** @fn const std::string& CE::Text::Font::reference() const
 * Return the stable application reference for saving a selected choice.
 * The returned string borrows the retained snapshot. An empty handle throws failed_operation.
 * @see [Choose and save a font](cheryl-guide:assets/text-layout#choose-and-save-a-font)
 */

/** @fn std::span<const CE::Text::FontFace> CE::Text::Font::faces() const
 * Borrow every retained primary/fallback style variant. Keep a copy of this Font alive while using the span or its byte spans. An empty handle throws failed_operation; no file access occurs.
 */

/** @fn CE::Text::FontCollection CE::Text::Font::collection(FontStyle style) const
 * Select one face per retained family, preferring matching italic state then nearest numeric weight; ties preserve retained order. The returned collection shares immutable bytes independently of this handle. Empty handles throw failed_operation and allocation failures propagate. No discovery, file access or graphics work occurs.
 */
