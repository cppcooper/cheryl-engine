#pragma once
#ifndef FRAME_H
#define FRAME_H
#include <cstddef>
#include <stdexcept>

namespace CE::Assets {
    struct Frame {
    protected:
        std::size_t offset_ = 0;
        std::size_t index_ = 0;
        std::size_t limit_ = 0;

    public:
        Frame(std::size_t o, std::size_t i, std::size_t l) :
            offset_(o), index_(i), limit_(l), offset(offset_), index(index_), limit(limit_) {}
        Frame(const Frame& other) :
            offset_(other.offset_), index_(other.index_), limit_(other.limit_), offset(offset_), index(index_),
            limit(limit_) {}
        Frame(Frame&& other) noexcept :
            offset_(other.offset_), index_(other.index_), limit_(other.limit_), offset(offset_), index(index_),
            limit(limit_) {}
        Frame& operator=(const Frame& other) {
            offset_ = other.offset_;
            index_ = other.index_;
            limit_ = other.limit_;
            return *this;
        }
        Frame& operator=(Frame&& other) noexcept {
            offset_ = other.offset_;
            index_ = other.index_;
            limit_ = other.limit_;
            return *this;
        }
        const std::size_t& offset;
        const std::size_t& index;
        const std::size_t& limit;
        template <typename T>
        T& operator[](const std::size_t frame) {
            set_frame(frame);
            return *reinterpret_cast<T*>(this);
        }
        void set_frame(const std::size_t frame) {
            if (limit_ == 0) {
                throw std::out_of_range("Cannot select a frame from an empty sequence");
            }
            index_ = frame % limit_;
        }
    };
}
#endif
