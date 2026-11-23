#pragma once

#include <cheryl/tools/console/runtime/runner.h>

namespace CE::Tools::Console::Runtime {
    namespace Detail { struct CommandState; }
    /** Separate privileged registration; the host retains Registry until close().
     * Bound requests retain the registered generation. Closing stops admission
     * and closes every admitted job without waiting; Work retains owned results.
     * Only host-issued Contexts permitting run_rights can invoke test.run.
     * The default bit 4 preserves the optional Contact adapter's existing grant.
     */
    class Commands {
        std::shared_ptr<Detail::CommandState> state_;
        Core::Registration registration_;
    public:
        Commands(Core::Registry&, Config host_config, Core::Rights run_rights = 4);
        Commands(const Commands&) = delete;
        Commands& operator=(const Commands&) = delete;
        ~Commands();
        void close() noexcept;
    };
}
