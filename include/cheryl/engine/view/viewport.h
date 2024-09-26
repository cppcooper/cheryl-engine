#pragma once
#include <glm.hpp>

namespace CE {
    template<typename T>
    struct ViewPort : protected glm::vec<2, T> {
        const T& width = this->x;
        const T& height = this->y;

        ViewPort(T x, T y) : glm::vec<2, T>(x, y) { }
    };
}
