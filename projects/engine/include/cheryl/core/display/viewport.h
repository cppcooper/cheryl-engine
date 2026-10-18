#pragma once
namespace CE::Platform {
/** Pair of dimensions with caller-selected representation and units.
     * @tparam T Stored width/height type; this value performs no positivity or range validation.
     */
    template <typename T> struct Dimensions {
        T width; ///< Width in the coordinate units defined by the containing API.
        T height; ///< Height in the same units as width.

        Dimensions(T width, T height)
        : width(width), height(height) {}
    };
}

/**
 * @fn template<typename T> CE::Platform::Dimensions<T>::Dimensions(T width, T height)
 * @brief Store the supplied width and height unchanged.
 * @param width Initial width; no normalization is applied.
 * @param height Initial height; validity belongs to the consuming operation.
 */
