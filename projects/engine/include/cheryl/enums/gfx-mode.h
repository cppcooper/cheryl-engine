#pragma once

namespace CE::Rendering {
    /** Graphics-dimensionality choice; it does not itself configure camera/depth/pipeline state. */
    enum class GraphicsMode { R2D = 0, ///< Two-dimensional graphics mode identifier.
        R3D ///< Three-dimensional graphics mode identifier.
    };
}
