#pragma once

#include <cheryl/core/resources/map-storage/codec.h>

namespace CE::Maps {
    // Supported Linux profile: existing caller-controlled directory, single writer,
    // regular-or-absent final entry. Final symlinks/nonregular entries reject.
    // Make absolute once, preserving components for native ancestor-symlink/..
    // lookup; diagnostics keep that path. Returned values own their data.
    // No renderer/asset lookup.
    [[nodiscard]] CE_API Snapshot load(const std::filesystem::path& path);

    // Encode completely, exclusively stage beside the target, check writes/close,
    // then rename once. Precommit errors preserve the target; a successful rename
    // returns success. No fsync, power-loss, hostile-race or multiwriter guarantee.
    // A published file has staging mode 0600; old permissions/ACLs are not copied.
    // Unsupported platforms reject before accessing the path. Error owns failure
    // details and any cleanup leftover; allocation/programming failures propagate.
    CE_API void save(const std::filesystem::path& path, const Snapshot& snapshot);
}
