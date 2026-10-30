#pragma once
#ifndef FRAME_H
#define FRAME_H
#include <cstddef>
#include <algorithm>
#include <type_traits>
#include <internals/exceptions.h>

namespace CE::Assets::Animation {
    /** Ordinal selection beyond the end of a nonempty sequence. */
    enum class IndexPolicy { Wrap, ///< Reduce a later selection modulo limit.
        Clamp ///< Hold the last ordinal for a later selection at or above limit.
    };

    /** Unsynchronized ordinal cursor, independent of elapsed time or grid cells.
     * A zero limit throws; the initial index must be below the limit. set_frame()
     * wraps/clamps later selections. offset() is metadata, not added to index().
     */
    template <typename Derived> struct Cursor {
        [[nodiscard]] std::size_t offset() const noexcept { return offset_; }
        [[nodiscard]] std::size_t index() const noexcept { return index_; }
        [[nodiscard]] std::size_t limit() const noexcept { return limit_; }
        [[nodiscard]] IndexPolicy index_policy() const noexcept { return index_policy_; }

        Derived& operator[](const std::size_t frame) {
            static_assert(std::is_base_of_v<Cursor, Derived>, "T must derive from Frame<T>");
            set_frame(frame);
            return static_cast<Derived&>(*this);
        }

        void set_frame(const std::size_t frame) noexcept {
            index_ = index_policy_ == IndexPolicy::Wrap ? frame % limit_ : std::min(frame, limit_ - 1);
        }

    protected:
        std::size_t offset_ = 0; ///< Derived-type metadata offset; never automatically added to index().
        std::size_t index_ = 0; ///< Current ordinal; derived implementations preserve index_ < limit_.
        std::size_t limit_ = 0; ///< Nonzero sequence length after successful construction; preserve for all modulo/clamp operations.
        IndexPolicy index_policy_; ///< Valid wrapping or clamping choice for subsequent set_frame() calls.

    public:
        Cursor(std::size_t offset, std::size_t index, std::size_t limit, IndexPolicy index_policy = IndexPolicy::Wrap)
        : offset_(offset), index_(index), limit_(limit), index_policy_(index_policy) {
            if (limit_ == 0) {
                throw Exceptions::bad_request(CE_HERE, "Cannot select a frame from an empty sequence");
            }
        }

        Cursor(const Cursor&) = default;
        Cursor(Cursor&&) noexcept = default;
        Cursor& operator=(const Cursor&) = default;
        Cursor& operator=(Cursor&&) noexcept = default;
    protected:
        ~Cursor() = default;
    };
}

#endif

/**
 * @fn template<typename Derived> CE::Assets::Animation::Cursor<Derived>::Cursor(std::size_t offset, std::size_t index, std::size_t limit, IndexPolicy index_policy)
 * @brief Store independent ordinal state for a nonempty sequence.
 * @param offset Derived metadata retained without arithmetic or validation.
 * @param index Initial ordinal; caller must ensure index < limit. It is not wrapped/clamped or checked here.
 * @param limit Positive sequence length.
 * @param index_policy Valid Wrap/Clamp policy for later selections.
 * @throws CE::Exceptions::bad_request Only when limit is zero.
 * No wall clock, grid, animation durations or resource ownership is created; mutation is unsynchronized.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn template<typename Derived> std::size_t CE::Assets::Animation::Cursor<Derived>::offset() const noexcept
 * @brief Read metadata offset without adding it to the current ordinal.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn template<typename Derived> std::size_t CE::Assets::Animation::Cursor<Derived>::index() const noexcept
 * @brief Read the current sequence ordinal, not necessarily a grid-cell ID.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn template<typename Derived> std::size_t CE::Assets::Animation::Cursor<Derived>::limit() const noexcept
 * @brief Read the positive ordinal limit retained by this cursor.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn template<typename Derived> CE::Assets::Animation::IndexPolicy CE::Assets::Animation::Cursor<Derived>::index_policy() const noexcept
 * @brief Read the wrapping/clamping rule used by later selections.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn template<typename Derived> Derived& CE::Assets::Animation::Cursor<Derived>::operator[](std::size_t frame)
 * @brief Apply set_frame() and return the derived cursor object.
 * @param frame Requested ordinal, wrapped or clamped by policy.
 * @pre Derived inherits from this Cursor specialization.
 * No duration-based advancement or resource lookup occurs.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn template<typename Derived> void CE::Assets::Animation::Cursor<Derived>::set_frame(std::size_t frame) noexcept
 * @brief Update the ordinal using modulo Wrap or last-entry Clamp policy.
 * @param frame Requested ordinal, including values at/above limit.
 * The derived type must preserve positive limit and valid policy. Offset metadata is unchanged and no timeline is advanced.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn template<typename Derived> CE::Assets::Animation::Cursor<Derived>::Cursor(const Cursor&)
 * @brief Copy ordinal, limit, policy and offset values; playback state becomes independent.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn template<typename Derived> CE::Assets::Animation::Cursor<Derived>::Cursor(Cursor&&) noexcept
 * @brief Move the scalar state; this base retains no owned resource or clock.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn template<typename Derived> Cursor<Derived>& CE::Assets::Animation::Cursor<Derived>::operator=(const Cursor&)
 * @brief Copy cursor metadata/state without advancing either sequence.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn template<typename Derived> Cursor<Derived>& CE::Assets::Animation::Cursor<Derived>::operator=(Cursor&&) noexcept
 * @brief Move scalar cursor state; resource ownership belongs to the derived value.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn template<typename Derived> CE::Assets::Animation::Cursor<Derived>::~Cursor()
 * @brief Destroy the base cursor through its derived owner; destruction advances no timeline.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback)
 */
