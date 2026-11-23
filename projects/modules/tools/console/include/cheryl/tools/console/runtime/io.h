#pragma once

#include <cheryl/tools/console/runtime/report.h>
#include <cheryl/core/engine/worker-pool.h>
#include <functional>

namespace CE::Tools::Console::Runtime {
    enum class IoOperation { None, Input, Snapshot, Finalize, Publish, Cleanup };
    enum class IoStatus { Pending, Input, Ready, Complete, Failed, Cancelled };
    enum class IoSubmit { Accepted, Busy, Full, Closed };
    struct IoInput {
        std::filesystem::path script_root, asset_root, output_root;
        std::string script, run_id, build_id;
        std::size_t capacity = max_output_bytes;
        std::string asset;
    };
    struct IoResult {
        IoStatus status = IoStatus::Pending;
        std::string source, asset;
        std::exception_ptr error;
    };
    namespace Detail { struct IoState; struct IoReceipt; }
    class IoHandle {
        std::shared_ptr<Detail::IoState> state_;
        std::shared_ptr<Detail::IoReceipt> receipt_;
        std::size_t index_ = 0;
        std::uint64_t generation_ = 0;
        IoHandle(std::shared_ptr<Detail::IoState>, std::shared_ptr<Detail::IoReceipt>, std::size_t, std::uint64_t);
        friend class IoService;
    public:
        IoHandle() = default;
        IoHandle(const IoHandle&) = delete;
        IoHandle& operator=(const IoHandle&) = delete;
        IoHandle(IoHandle&&) noexcept;
        IoHandle& operator=(IoHandle&&) noexcept;
        ~IoHandle();
        explicit operator bool() const noexcept { return static_cast<bool>(state_); }
        IoResult poll();
        IoSubmit submit(std::string json, bool final, std::string source_id);
        IoStatus close() noexcept;
    };
    /** One accepted Engine Group callback owns every file and cleanup obligation.
     * Handles never wait for I/O. The host retains its Pool outside UI teardown,
     * closes admission, retires handles, calls settle() from its shutdown owner,
     * and only then shuts down the Pool. settle() can wait indefinitely for a
     * stalled OS syscall; it must never run from Job/Commands or a pool worker.
     * Gate is an optional lane-thread test seam, invoked without mailbox locks.
     */
    class IoService {
        std::shared_ptr<Detail::IoState> state_;
    public:
        static constexpr std::size_t max_jobs = 8;
        static constexpr std::size_t max_queued_bytes = max_jobs * 2 * Script::max_file_bytes;
        using Gate = std::function<void(IoOperation)>;
        explicit IoService(CE::Workers::Group, Gate = {}, std::size_t byte_capacity = max_queued_bytes);
        IoService(const IoService&) = delete;
        IoService& operator=(const IoService&) = delete;
        ~IoService();
        IoHandle admit(IoInput);
        void close_admission() noexcept;
        void shutdown() noexcept;
        void settle();
        bool settled() const noexcept;
        std::size_t retained() const noexcept;
    };
}
