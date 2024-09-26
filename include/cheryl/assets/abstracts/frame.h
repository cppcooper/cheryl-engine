#pragma once
#ifndef FRAME_H
#define FRAME_H
#include <cinttypes>

namespace CE::Assets {
    struct Frame {
    protected:
        uint16_t offset_ = 0;
        uint16_t index_ = 0;
        uint16_t limit_ = 0;
    public:
        Frame(uint16_t o, uint16_t i, uint16_t l) : offset_(o), index_(i), limit_(l) {}
        const uint16_t &offset = offset_;
        const uint16_t &index = index_;
        const uint16_t &limit = limit_;
        template<typename T>
        T& operator[](const std::size_t frame) {
            set_frame(frame);
            return *reinterpret_cast<T*>(this);
        }
        void set_frame(const std::size_t frame) {
            index_ = frame % limit;
        }
    };
}
#endif
