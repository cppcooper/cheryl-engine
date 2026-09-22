#pragma once
namespace CE {
    template <typename T>
    struct ViewPort {
        T width;
        T height;

        ViewPort(T width, T height) : width(width), height(height) {}
    };
}
