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
        glm::mat4 projection{1.0f}; ///< Copied pass projection; no live camera is consulted.
        glm::mat4 view{1.0f}; ///< Copied pass world-to-view transform.
        ::CE::Rendering::Parameters::Set parameters; ///< Owned custom layer used when resolving draws.
        ::CE::Rendering::Pipelines::PassConstraints2D constraints; ///< Required pipeline fixed-state choices, not state overrides.
        std::vector<::CE::Rendering::DrawPacket2D> draws; ///< Owned resolved packets played in authored order.
    };

    class FrameWriter;
    class PassWriter;

    /** One reusable storage slot. Only the active passes are published; inactive pass
     * objects keep their draw-vector capacity for later ticks. Runtime ownership must
     * prevent writing or recycling while the renderer reads this frame.
     * Frames are neither copyable nor movable; transfer slot ownership, not this object.
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

        void add(::CE::Rendering::DrawPacket2D draw) {
            auto& pass = frame_.passes_[index_];
            ::CE::Rendering::validate_draw_packet(draw, pass.constraints);
            draw.authored_order = pass.draws.size();
            pass.draws.push_back(std::move(draw));
        }

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

/**
 * @fn CE::Rendering::Frame::Frame()
 * @brief Create an empty unpublished reusable slot; it owns no active passes.
 */

/**
 * @fn std::span<const CE::Rendering::Pass> CE::Rendering::Frame::passes() const
 * @return Borrowed active passes only. Retain this frame and exclude writers/recycling for the entire read; outer-vector growth invalidates the span.
 */

/**
 * @fn void CE::Rendering::Frame::recycle()
 * @brief Release active draw handles/custom values and mark the slot empty while retaining vector capacity.
 * Call only after rendering or supersession excludes readers, on the resource-release owner required by the runtime/backend. Writers must not still use this slot; this operation does not synchronize or publish it.
 */

/**
 * @fn void CE::Rendering::PassWriter::reserve_draws(std::size_t count)
 * @brief Reserve packet capacity for this active preparation pass.
 * @param count Desired vector capacity, measured in packets.
 * Allocation/length failures propagate without changing existing draws; no frame publication occurs.
 */

/**
 * @fn CE::Rendering::Parameters::PassSemantics CE::Rendering::PassWriter::semantics() const
 * @return Independent copy of this pass's projection/view matrices for CPU packet resolution.
 */

/**
 * @fn const CE::Rendering::Parameters::Set& CE::Rendering::PassWriter::parameters() const
 * @return Borrowed custom pass values. Outer-vector growth or recycling invalidates the reference; keep the writer in its preparation phase.
 */

/**
 * @fn const CE::Rendering::Pipelines::PassConstraints2D& CE::Rendering::PassWriter::constraints() const
 * @return Borrowed pass requirements, invalidated by outer-vector growth/recycling; no state override or synchronization occurs.
 */

/**
 * @fn void CE::Rendering::PassWriter::add(CE::Rendering::DrawPacket2D draw)
 * @brief Validate and append one owned packet in authored order.
 * @param draw Resolved values retaining resources required for later playback.
 * Assigns authored_order from the current draw count. Validation/allocation failures leave existing draws intact; no backend calls or cross-thread publication occur.
 */

/**
 * @fn void CE::Rendering::PassWriter::add(std::vector<::CE::Rendering::DrawPacket2D> draws)
 * @brief Validate an entire group before appending any packets in its existing order.
 * @param draws Owned glyph/asset packet group.
 * Validation/storage-limit/allocation failures leave prior draws intact. Appended packets receive consecutive authored_order values; empty groups do nothing. No backend call/publication occurs.
 */

/**
 * @fn CE::Rendering::FrameWriter::FrameWriter(CE::Rendering::Frame& frame)
 * @brief Borrow one free slot for exclusive CPU preparation.
 * @param frame Live frame excluded from all other readers/writers.
 * @throws CE::Exceptions::failed_operation If active passes have not been recycled.
 * Destruction neither publishes nor rolls back preparation; after a failed preparation recycle the partial frame before reuse.
 */

/**
 * @fn void CE::Rendering::FrameWriter::reserve_passes(std::size_t count)
 * @brief Reserve outer pass capacity before preparing draws.
 * @param count Desired pass capacity.
 * Growth invalidates borrowed pass references/spans, while PassWriter indices remain usable. Allocation/length failures propagate; no synchronization/publication occurs.
 */

/**
 * @fn CE::Rendering::PassWriter CE::Rendering::FrameWriter::begin_pass(const glm::mat4& projection, const glm::mat4& view, CE::Rendering::Pipelines::PassConstraints2D constraints, CE::Rendering::Parameters::Set parameters)
 * @brief Append an active pass, copying matrices and owning parameter/constraint values.
 * @param projection Projection copied into pass semantics.
 * @param view World-to-view matrix copied into pass semantics.
 * @param constraints Fixed-state requirements checked against submitted pipelines.
 * @param parameters Owned lower-priority custom parameter layer.
 * @return Writer borrowing the new pass through this preparation phase, before recycle/publication.
 * Allocation failures propagate; no backend call or cross-thread publication occurs.
 * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
 */
