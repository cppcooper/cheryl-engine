#include <cheryl/tools/console/runtime/io.h>

#include <array>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace CE::Tools::Console::Runtime {
    namespace Detail {
        struct IoReceipt { bool committed = false; };
        struct IoSlot {
            IoInput input;
            IoResult result;
            std::string json, source_id;
            std::shared_ptr<IoReceipt> receipt;
            std::uint64_t generation = 0;
            std::size_t charge = 0;
            IoOperation operation = IoOperation::None;
            bool used = false, retired = false, working = false, failed = false;
        };
        struct IoState {
            std::mutex mutex;
            std::condition_variable wake;
            std::array<IoSlot, IoService::max_jobs> slots;
            std::shared_future<void> lane;
            std::size_t bytes = 0, cursor = 0, byte_capacity = IoService::max_queued_bytes;
            bool accepting = true, stopping = false;
        };
    }
    namespace {
        void release_charge(Detail::IoState& state, Detail::IoSlot& slot) {
            state.bytes -= slot.charge;
            slot.charge = 0;
        }
        void run_lane(const std::shared_ptr<Detail::IoState>& state, const IoService::Gate& gate) {
            std::array<std::unique_ptr<Report>, IoService::max_jobs> reports;
            std::unique_lock lock(state->mutex);
            for (;;) {
                state->wake.wait(lock, [&] {
                    if (state->stopping) return true;
                    for (const auto& slot : state->slots)
                        if (slot.used && (slot.retired || slot.operation != IoOperation::None)) return true;
                    return false;
                });
                std::size_t index = state->slots.size();
                for (std::size_t offset = 0; offset < state->slots.size(); ++offset) {
                    const auto candidate = (state->cursor + offset) % state->slots.size();
                    const auto& slot = state->slots[candidate];
                    if (slot.used && (slot.retired || slot.operation != IoOperation::None)) { index = candidate; break; }
                }
                if (index == state->slots.size()) {
                    if (state->stopping) return;
                    continue;
                }
                state->cursor = (index + 1) % state->slots.size();
                auto& slot = state->slots[index];
                const auto operation = slot.retired ? IoOperation::Cleanup : slot.operation;
                const bool preserve = slot.receipt->committed;
                auto input = std::move(slot.input);
                auto json = std::move(slot.json);
                auto source_id = std::move(slot.source_id);
                slot.operation = IoOperation::None;
                slot.working = true;
                lock.unlock();
                IoResult result;
                bool cleanup_done = false;
                try {
                    if (gate) gate(operation);
                    switch (operation) {
                    case IoOperation::Input:
                        result.source = read_source(input.script_root, input.script);
                        result.asset = (input.asset.empty() ? std::string{} : read_source(input.asset_root, input.asset));
                        reports[index] = std::make_unique<Report>(input.output_root, input.run_id, input.capacity);
                        reports[index]->transcript(result.source + '\n');
                        result.status = IoStatus::Input;
                        break;
                    case IoOperation::Snapshot:
                    case IoOperation::Finalize:
                        reports[index]->transcript("snapshot " + json + '\n');
                        if (operation == IoOperation::Finalize) reports[index]->finish(json, source_id, input.build_id);
                        else reports[index]->running(json);
                        if (operation == IoOperation::Finalize && gate) gate(IoOperation::Publish);
                        result.status = operation == IoOperation::Finalize ? IoStatus::Complete : IoStatus::Ready;
                        break;
                    case IoOperation::Cleanup:
                        if (reports[index] && !preserve && !reports[index]->invalidate())
                            throw std::runtime_error("Scenario marker cleanup remains pending");
                        reports[index].reset();
                        cleanup_done = true;
                        result.status = IoStatus::Cancelled;
                        break;
                    case IoOperation::None:
                    case IoOperation::Publish: break;
                    }
                } catch (...) {
                    result.status = IoStatus::Failed;
                    result.error = std::current_exception();
                    if (operation != IoOperation::Cleanup && reports[index] && reports[index]->invalidate()) reports[index].reset();
                }
                if (operation == IoOperation::Cleanup && !cleanup_done) std::this_thread::sleep_for(std::chrono::milliseconds(10));
                lock.lock();
                slot.working = false;
                slot.input = std::move(input);
                if (operation == IoOperation::Cleanup && cleanup_done) {
                    release_charge(*state, slot);
                    slot.result = {};
                    slot.input = {};
                    slot.receipt.reset();
                    slot.used = false;
                } else if (slot.retired) {
                    slot.result = {};
                } else {
                    if (result.status == IoStatus::Complete) slot.receipt->committed = true;
                    if (result.status == IoStatus::Failed) slot.failed = true;
                    slot.result = std::move(result);
                }
                state->wake.notify_all();
            }
        }
    }
    IoHandle::IoHandle(std::shared_ptr<Detail::IoState> state, std::shared_ptr<Detail::IoReceipt> receipt,
        std::size_t index, std::uint64_t generation)
        : state_(std::move(state)), receipt_(std::move(receipt)), index_(index), generation_(generation) {}
    IoHandle::IoHandle(IoHandle&& other) noexcept
        : state_(std::move(other.state_)), receipt_(std::move(other.receipt_)), index_(other.index_), generation_(other.generation_) {}
    IoHandle& IoHandle::operator=(IoHandle&& other) noexcept {
        if (this != &other) { close(); state_ = std::move(other.state_); receipt_ = std::move(other.receipt_); index_ = other.index_; generation_ = other.generation_; }
        return *this;
    }
    IoHandle::~IoHandle() { close(); }
    IoResult IoHandle::poll() {
        if (!state_) return {IoStatus::Cancelled, {}, {}, {}};
        std::lock_guard lock(state_->mutex);
        if (receipt_->committed) return {IoStatus::Complete, {}, {}, {}};
        auto& slot = state_->slots[index_];
        if (!slot.used || slot.generation != generation_ || slot.retired) return {IoStatus::Cancelled, {}, {}, {}};
        if (state_->lane.valid() && state_->lane.wait_for(std::chrono::seconds(0)) == std::future_status::ready &&
            slot.result.status != IoStatus::Complete && slot.result.status != IoStatus::Failed) {
            try { state_->lane.get(); } catch (...) { return {IoStatus::Failed, {}, {}, std::current_exception()}; }
            return {IoStatus::Cancelled, {}, {}, {}};
        }
        if (slot.working || slot.operation != IoOperation::None) return {};
        auto result = std::move(slot.result);
        slot.result.status = result.status == IoStatus::Complete ? IoStatus::Complete : IoStatus::Pending;
        release_charge(*state_, slot);
        return result;
    }
    IoSubmit IoHandle::submit(std::string json, bool final, std::string source_id) {
        if (!state_) return IoSubmit::Closed;
        if (json.size() > Script::max_file_bytes || source_id.size() > Core::max_text) return IoSubmit::Full;
        std::lock_guard lock(state_->mutex);
        auto& slot = state_->slots[index_];
        if (!slot.used || slot.generation != generation_ || slot.retired) return IoSubmit::Closed;
        if (slot.failed || slot.receipt->committed || state_->stopping ||
            state_->lane.wait_for(std::chrono::seconds(0)) == std::future_status::ready) return IoSubmit::Closed;
        if (slot.working || slot.operation != IoOperation::None || slot.result.status != IoStatus::Pending) return IoSubmit::Busy;
        if (json.size() > state_->byte_capacity - state_->bytes) return IoSubmit::Full;
        slot.charge = json.size(); state_->bytes += slot.charge;
        slot.json = std::move(json); slot.source_id = std::move(source_id);
        slot.operation = final ? IoOperation::Finalize : IoOperation::Snapshot;
        state_->wake.notify_one();
        return IoSubmit::Accepted;
    }
    IoStatus IoHandle::close() noexcept {
        if (!state_) return IoStatus::Cancelled;
        std::lock_guard lock(state_->mutex);
        auto& slot = state_->slots[index_];
        const auto committed = receipt_->committed;
        if (!slot.used || slot.generation != generation_) return committed ? IoStatus::Complete : IoStatus::Cancelled;
        const auto status = committed ? IoStatus::Complete : slot.result.status;
        slot.retired = true;
        state_->wake.notify_one();
        return status;
    }
    IoService::IoService(CE::Workers::Group group, Gate gate, std::size_t byte_capacity) : state_(std::make_shared<Detail::IoState>()) {
        if (byte_capacity < 2 * Script::max_file_bytes || byte_capacity > max_queued_bytes)
            throw std::runtime_error("Invalid scenario I/O byte capacity");
        state_->byte_capacity = byte_capacity;
        auto lane = group.submit([state = state_, gate = std::move(gate)] { run_lane(state, gate); });
        state_->lane = lane.share();
    }
    IoService::~IoService() { shutdown(); }
    IoHandle IoService::admit(IoInput input) {
        if (input.script.size() > Core::max_text || input.run_id.size() > 64 || input.build_id.empty() ||
            input.build_id.size() > Core::max_text || !Core::valid_utf8(input.build_id) ||
            input.script_root.native().size() > Core::max_text || input.asset_root.native().size() > Core::max_text ||
            input.output_root.native().size() > Core::max_text) throw std::runtime_error("Invalid bounded I/O request");
        std::lock_guard lock(state_->mutex);
        constexpr auto charge = 2 * Script::max_file_bytes;
        if (!state_->accepting || settled() || charge > state_->byte_capacity - state_->bytes) return {};
        for (std::size_t index = 0; index < state_->slots.size(); ++index) {
            auto& slot = state_->slots[index];
            if (slot.used) continue;
            auto receipt = std::make_shared<Detail::IoReceipt>();
            slot.receipt = receipt;
            slot.input = std::move(input); slot.result = {}; slot.used = true; slot.retired = false;
            slot.failed = false;
            slot.operation = IoOperation::Input; slot.charge = charge; state_->bytes += charge;
            ++slot.generation;
            state_->wake.notify_one();
            return IoHandle(state_, std::move(receipt), index, slot.generation);
        }
        return {};
    }
    void IoService::close_admission() noexcept { std::lock_guard lock(state_->mutex); state_->accepting = false; }
    void IoService::shutdown() noexcept {
        std::lock_guard lock(state_->mutex);
        state_->accepting = false; state_->stopping = true;
        for (auto& slot : state_->slots) if (slot.used) slot.retired = true;
        state_->wake.notify_one();
    }
    bool IoService::settled() const noexcept { return state_->lane.valid() && state_->lane.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }
    void IoService::settle() {
        shutdown();
        state_->lane.wait();
        state_->lane.get();
    }
    std::size_t IoService::retained() const noexcept {
        std::lock_guard lock(state_->mutex);
        std::size_t count = 0;
        for (const auto& slot : state_->slots) if (slot.used) ++count;
        return count;
    }
}
