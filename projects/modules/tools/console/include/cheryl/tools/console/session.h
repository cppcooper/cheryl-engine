#pragma once

#include <cheryl/tools/console/registry.h>

namespace CE::Tools::Console {
    class Session {
        struct Entry {
            std::uint64_t id;
            BoundRequest request;
            Result queued;
            std::shared_ptr<Work> work;
        };
        Context context_;
        std::vector<Entry> entries_;
        std::size_t capacity_;
        bool accepting_ = true, pumping_ = false;
        const Result& result(const Entry&) const noexcept;
    public:
        explicit Session(Context context, std::size_t capacity = max_requests);
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        ~Session();
        // Result pointers are borrowed; reacquire through lookup() after each pump.
        // Retained IDs never evict, including after close; use a new session at capacity.
        Checked<const Result*> submit(std::uint64_t id, const BoundRequest&);
        Checked<const Result*> lookup(std::uint64_t id) const noexcept;
        Checked<void> pump() noexcept;
        void close() noexcept;
        const Context& context() const noexcept { return context_; }
        std::size_t retained() const noexcept { return entries_.size(); }
        bool accepting() const noexcept { return accepting_ && context_.live() && !pumping_; }
    };
}
