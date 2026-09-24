#pragma once
#ifndef FRAME_H
#define FRAME_H
#include <cstddef>
#include <internals/exceptions.h>

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
        // TODO: Prefer accessors over public reference aliases. Reference members force the
        // hand-written copy/move operations above and make Frame's value semantics unusually fragile.
        const std::size_t& offset;
        const std::size_t& index;
        const std::size_t& limit;
        template <typename T>
        T& operator[](const std::size_t frame) {
            set_frame(frame);
            // TODO: Replace this unchecked reinterpret_cast with a type-safe frame-selection API.
            // Frame is used as a secondary base (for example by Tileset), so its subobject address
            // is not guaranteed to equal the complete T object's address under multiple inheritance.
            return *reinterpret_cast<T*>(this);
        }
        void set_frame(const std::size_t frame) {
            if (limit_ == 0) {
                throw Exceptions::bad_request(CE_HERE, "Cannot select a frame from an empty sequence");
            }
            index_ = frame % limit_;
        }
    };
}
#endif
