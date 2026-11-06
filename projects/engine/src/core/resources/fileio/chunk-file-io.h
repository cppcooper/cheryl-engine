#pragma once
#include <cheryl/core/resources/fileio/chunk-file.h>
#include <ios>

namespace CE::Files::Chunks::Detail {
    // Shared with the deterministic close-failure probe; real file streams use
    // the same checked path rather than relying on their noexcept destructors.
    template <class File> void close_checked(File& file, std::uint64_t offset) {
        try {
            file.close();
        } catch (const std::ios_base::failure&) {
            throw Error(ErrorCode::io, offset, "file close failed");
        }
        if (file.fail())
            throw Error(ErrorCode::io, offset, "file close failed");
    }
}
