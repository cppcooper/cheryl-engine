#pragma once
namespace CE::Platform {
    template <typename T> struct Dimensions {
        T width;
        T height;

        Dimensions(T width, T height)
        : width(width), height(height) {}
    };
}
