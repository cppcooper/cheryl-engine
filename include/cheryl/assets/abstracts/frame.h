#pragma once
#ifndef FRAME_H
#define FRAME_H
#include <cstddef>
#include <internals/exceptions.h>

namespace CE::Assets {
    enum class FrameIndexPolicy {
        Wrap,
        Clamp
    };

    template <typename Derived>
    struct Frame {
        [[nodiscard]] std::size_t offset() const noexcept { return offset_; }
        [[nodiscard]] std::size_t index() const noexcept { return index_; }
        [[nodiscard]] std::size_t limit() const noexcept { return limit_; }
        [[nodiscard]] FrameIndexPolicy index_policy() const noexcept { return index_policy_; }

        Derived& operator[](const std::size_t frame) {
            static_assert(std::is_base_of_v<Frame, Derived>, "T must derive from Frame<T>");
            set_frame(frame);
            return static_cast<Derived&>(*this);
        }

        void set_frame(const std::size_t frame) noexcept {
            index_ = index_policy_ == FrameIndexPolicy::Wrap
                         ? frame % limit_
                         : std::min(frame, limit_ - 1);
        }

    protected:
        std::size_t offset_ = 0;
        std::size_t index_ = 0;
        std::size_t limit_ = 0;
        FrameIndexPolicy index_policy_;

    public:
        Frame(
            std::size_t offset,
            std::size_t index,
            std::size_t limit,
            FrameIndexPolicy index_policy = FrameIndexPolicy::Wrap
        ) :
            offset_(offset), index_(index), limit_(limit), index_policy_(index_policy) {
            if (limit_ == 0) {
                throw Exceptions::bad_request(CE_HERE, "Cannot select a frame from an empty sequence");
            }
        }

        Frame(const Frame&) = default;
        Frame(Frame&&) noexcept = default;
        Frame& operator=(const Frame&) = default;
        Frame& operator=(Frame&&) noexcept = default;
        virtual ~Frame() = default;
    };
}
#endif
