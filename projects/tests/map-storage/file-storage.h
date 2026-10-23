#pragma once

#include "codec.h"

namespace MapFixture {
    // Private Linux profile: existing caller-controlled directory, single writer,
    // regular-or-absent final entry. Final symlinks/nonregular entries reject.
    // Resolve once; returned values own their data. No renderer/asset lookup.
    [[nodiscard]] Snapshot load(const std::filesystem::path& path);

    // Encode completely, exclusively stage beside the target, check writes/close,
    // then rename once. Precommit errors preserve the target; a successful rename
    // returns success. No fsync, power-loss, hostile-race or multiwriter guarantee.
    // A published file has staging mode 0600; old permissions/ACLs are not copied.
    // Unsupported platforms reject before accessing the path. Error owns failure
    // details and any cleanup leftover; allocation/programming failures propagate.
    void save(const std::filesystem::path& path, const Snapshot& snapshot);
}
