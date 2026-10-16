#pragma once
#ifndef FRAME_H
#define FRAME_H
#include <cstddef>
#include <algorithm>
#include <type_traits>
#include <internals/exceptions.h>

namespace CE::Assets::Animation {
    enum class IndexPolicy { Wrap, Clamp };

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
        std::size_t offset_ = 0;
        std::size_t index_ = 0;
        std::size_t limit_ = 0;
        IndexPolicy index_policy_;

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
