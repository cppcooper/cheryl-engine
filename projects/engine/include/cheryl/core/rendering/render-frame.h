#pragma once

#include <core/rendering/draw-packet.h>

#include <glm.hpp>
#include <internals/exceptions.h>

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace CE::Rendering {
    /** One ordered pass. Matrices are copied from the simulation's camera; the
     * renderer never observes a live CameraBase or animation playback cursor.
     */
    struct Pass {
        glm::mat4 projection{1.0f};
        glm::mat4 view{1.0f};
        ::CE::Rendering::Parameters::Set parameters;
        ::CE::Rendering::Pipelines::PassConstraints2D constraints;
        std::vector<::CE::Rendering::DrawPacket2D> draws;
    };

    class FrameWriter;
    class PassWriter;

    /** One reusable storage slot. Only the active passes are published; inactive pass
     * objects keep their draw-vector capacity for later ticks. Runtime ownership must
     * prevent writing or recycling while the renderer reads this frame.
     */
    class Frame final {
    private:
        friend class ::CE::Rendering::FrameWriter;
        friend class ::CE::Rendering::PassWriter;

        std::vector<::CE::Rendering::Pass> passes_;
        std::size_t active_passes_ = 0;

    public:
        Frame() = default;
        Frame(const Frame&) = delete;
        Frame& operator=(const Frame&) = delete;
        Frame(Frame&&) = delete;
        Frame& operator=(Frame&&) = delete;

        // Borrowed active-pass view; keep the slot alive and exclude writing/recycling
        // for the entire read. Outer-vector growth invalidates existing views.
        [[nodiscard]] std::span<const ::CE::Rendering::Pass> passes() const { return {passes_.data(), active_passes_}; }

        // After rendering or supersession, release packet handles on the graphics
        // thread while its context is current, then return the slot to simulation.
        void recycle() {
            for (std::size_t i = 0; i < active_passes_; ++i) {
                passes_[i].draws.clear();
                passes_[i].parameters.clear();
            }
            active_passes_ = 0;
        }
    };

    /** A short-lived handle to one pass. Its index stays valid if adding another pass
     * grows the frame's outer vector. Use it only while the frame is being prepared.
     * No writer operation synchronizes or publishes a slot to another thread.
     */
    class PassWriter final {
    private:
        friend class ::CE::Rendering::FrameWriter;
        ::CE::Rendering::Frame& frame_;
        std::size_t index_;

    public:
        void reserve_draws(std::size_t count) { frame_.passes_[index_].draws.reserve(count); }

        [[nodiscard]] ::CE::Rendering::Parameters::PassSemantics semantics() const {
            const auto& pass = frame_.passes_[index_];
            return {pass.projection, pass.view};
        }
        // Borrowed references can be invalidated by outer-vector growth or recycling.
        [[nodiscard]] const ::CE::Rendering::Parameters::Set& parameters() const { return frame_.passes_[index_].parameters; }
        [[nodiscard]] const ::CE::Rendering::Pipelines::PassConstraints2D& constraints() const {
            return frame_.passes_[index_].constraints;
        }

        /** Validate and append one owned packet in authored order.
         * Validation/allocation failure leaves the existing draw list intact.
         * @param draw Packet retaining the resources needed for later rendering.
         */
        void add(::CE::Rendering::DrawPacket2D draw) {
            auto& pass = frame_.passes_[index_];
            ::CE::Rendering::validate_draw_packet(draw, pass.constraints);
            draw.authored_order = pass.draws.size();
            pass.draws.push_back(std::move(draw));
        }

        /** Validate a complete group before inserting any glyph/asset packets.
         * Validation/storage-limit failures leave existing draws intact.
         * @param draws Owned packet group appended in its existing order.
         */
        void add(std::vector<::CE::Rendering::DrawPacket2D> draws) {
            auto& pass = frame_.passes_[index_];
            for (const auto& draw : draws)
                ::CE::Rendering::validate_draw_packet(draw, pass.constraints);
            if (draws.size() > pass.draws.max_size() - pass.draws.size())
                throw ::CE::Exceptions::invalid_args(CE_HERE, "Render pass exceeds packet storage limits");
            pass.draws.reserve(pass.draws.size() + draws.size());
            for (auto& draw : draws) {
                draw.authored_order = pass.draws.size();
                pass.draws.push_back(std::move(draw));
            }
        }

    private:
        PassWriter(::CE::Rendering::Frame& frame, std::size_t index)
        : frame_(frame), index_(index) {}
    };

    /** Writes into a free slot after update(). Camera matrices are copied once per
     * pass; draw packets are constructed in the slot's retained vector storage.
     * Caller excludes other writers/readers and retains the frame. Construction
     * rejects unrecycled active passes. Destruction does not publish or roll back;
     * a preparation failure requires recycling the partial frame before reuse.
     */
    class FrameWriter final {
    private:
        ::CE::Rendering::Frame& frame_;

    public:
        explicit FrameWriter(::CE::Rendering::Frame& frame)
        : frame_(frame) {
            if (frame_.active_passes_ != 0)
                throw ::CE::Exceptions::failed_operation(CE_HERE, "Render frame must be recycled before writing again");
        }

        void reserve_passes(std::size_t count) { frame_.passes_.reserve(count); }

        /** Append an active pass in authored order, copying matrices and owning values.
         * Keep the returned writer within this preparation phase, before recycling
         * or publication. No backend calls or cross-thread publication occur here.
         * @param projection Projection matrix copied into pass semantics.
         * @param view View matrix copied into pass semantics.
         * @param constraints Pass requirements used to validate submitted packets.
         * @param parameters Owned pass parameter values.
         * @return Writer borrowing this frame's new pass.
         */
        [[nodiscard]] ::CE::Rendering::PassWriter begin_pass(
            const glm::mat4& projection,
            const glm::mat4& view,
            ::CE::Rendering::Pipelines::PassConstraints2D constraints = ::CE::Rendering::Pipelines::PassConstraints2D{},
            ::CE::Rendering::Parameters::Set parameters = ::CE::Rendering::Parameters::Set{}
        ) {
            const auto index = frame_.active_passes_;
            if (index == frame_.passes_.size())
                frame_.passes_.emplace_back();
            auto& pass = frame_.passes_[index];
            pass.projection = projection;
            pass.view = view;
            pass.constraints = std::move(constraints);
            pass.parameters = std::move(parameters);
            ++frame_.active_passes_;
            return ::CE::Rendering::PassWriter(frame_, index);
        }
    };
}
