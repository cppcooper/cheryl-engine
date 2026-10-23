#pragma once

#include "codec.h"

namespace GateConsole {
    namespace Testing {
        // Private, fixed boundary seam. Failures are simulated without exhausting
        // resources or assuming permissions. No reusable I/O dispatch framework.
        struct FileFailures {
            bool open = false;
            bool partial_write = false;
            bool flush = false;
            bool close = false;
            bool cleanup = false;
        };
    }

    std::expected<std::string, std::string> read_save(const std::string& path);
    std::expected<void, std::string> save_fresh(Encounter& encounter, const std::string& path, Testing::FileFailures failures = {});
    std::expected<void, std::string> resume_file(Encounter& encounter, const std::string& path);
}
