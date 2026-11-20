#include <cheryl/tools/console/session.h>
#include "internal.h"

#include <algorithm>
#include <new>
#include <stdexcept>

namespace CE::Tools::Console {
    Session::Session(Context context, std::size_t capacity) : context_(std::move(context)), capacity_(capacity) {
        if (!capacity || capacity > max_requests) throw std::invalid_argument("Invalid console request capacity");
        entries_.reserve(capacity);
    }
    Session::~Session() { close(); }
    const Result& Session::result(const Entry& entry) const noexcept { return entry.work ? entry.work->result : entry.queued; }
    Checked<const Result*> Session::submit(std::uint64_t id, const BoundRequest& request) {
        for (const auto& entry : entries_) if (entry.id == id) {
            if (!entry.request.same_body(request)) return std::unexpected(Failure{Code::Conflict});
            return &result(entry);
        }
        if (pumping_) return std::unexpected(Failure{Code::Reentrant});
        if (!accepting_ || !context_.live()) return std::unexpected(Failure{Code::Expired});
        if (!request.entry_ || !request.context_.same_scope(context_)) return std::unexpected(Failure{Code::Unauthorized});
        if (!request.entry_->active) return std::unexpected(Failure{Code::Unregistered});
        if (!id || entries_.size() == capacity_) return std::unexpected(Failure{Code::Capacity});
        try {
            entries_.push_back({id, request, {}, {}});
            return &entries_.back().queued;
        } catch (const std::bad_alloc&) { return std::unexpected(Failure{Code::Allocation}); }
    }
    Checked<const Result*> Session::lookup(std::uint64_t id) const noexcept {
        for (const auto& entry : entries_) if (entry.id == id) return &result(entry);
        return std::unexpected(Failure{context_.live() ? Code::Arguments : Code::Expired});
    }
    Checked<void> Session::pump() noexcept {
        if (pumping_) return std::unexpected(Failure{Code::Reentrant});
        pumping_ = true;
        for (auto& entry : entries_) {
            if (terminal(result(entry).stage)) continue;
            if (!entry.work) {
                if (!context_.live() || !entry.request.entry_->active || !accepting_) {
                    entry.queued.stage = Stage::Cancelled;
                    entry.queued.failure = {context_.live() ? Code::Unregistered : Code::Expired};
                    continue;
                }
                try {
                    entry.work = entry.request.entry_->handler(context_, entry.request.arguments_);
                    if (!entry.work) {
                        entry.queued.stage = Stage::Failed;
                        entry.queued.failure = {Code::Owner};
                        continue;
                    }
                } catch (const std::bad_alloc&) {
                    entry.queued.stage = Stage::Failed; entry.queued.failure = {Code::Allocation}; continue;
                } catch (...) {
                    entry.queued.stage = Stage::Failed; entry.queued.failure = {Code::Owner}; continue;
                }
            }
            entry.work->step();
            if (entry.work->result.stage == Stage::Pending || entry.work->result.stage == Stage::Succeeded ||
                entry.work->result.stage == Stage::Refused) {
                const auto checked = validate(entry.work->result.value, entry.request.descriptor().result, context_);
                if (!checked) {
                    entry.work->close();
                    entry.work->result.stage = Stage::Failed;
                    entry.work->result.failure = checked.error();
                }
            }
        }
        pumping_ = false;
        if (!accepting_) close();
        return {};
    }
    void Session::close() noexcept {
        accepting_ = false;
        if (pumping_) return;
        pumping_ = true;
        for (auto& entry : entries_) {
            if (terminal(result(entry).stage)) continue;
            if (entry.work) entry.work->close();
            else { entry.queued.stage = Stage::Cancelled; entry.queued.failure = {Code::Cancelled}; }
        }
        pumping_ = false;
    }
}
