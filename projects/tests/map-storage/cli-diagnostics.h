#pragma once

#include "codec.h"

#include <iomanip>
#include <ostream>
#include <string_view>

namespace MapFixture::Cli {
    [[nodiscard]] inline std::string_view category(Code code) {
        switch (code) {
            case Code::Read: return "Read";
            case Code::Malformed: return "Malformed";
            case Code::UnsupportedFormat: return "UnsupportedFormat";
            case Code::UnsupportedVersion: return "UnsupportedVersion";
            case Code::InvalidValue: return "InvalidValue";
            case Code::LimitExceeded: return "LimitExceeded";
            case Code::Open: return "Open";
            case Code::Write: return "Write";
            case Code::Replace: return "Replace";
            case Code::UnsupportedPlatform: return "UnsupportedPlatform";
        }
        return "Unknown";
    }

    inline void print_error(std::ostream& output, const Error& error) {
        output << "storage category=" << category(error.code)
               << " operation=" << std::quoted(error.operation)
               << " origin=" << std::quoted(error.origin.string())
               << " location=" << std::quoted(error.location);
        if (error.byte_offset)
            output << " byte_offset=" << *error.byte_offset;
        if (error.io_error)
            output << " io_error=" << error.io_error.category().name() << ':' << error.io_error.value()
                   << ' ' << std::quoted(error.io_error.message());
        output << "\ndetail: " << error.what() << '\n';
        if (error.cleanup_error || !error.staging_leftover.empty()) {
            output << "cleanup_error=" << error.cleanup_error.category().name() << ':' << error.cleanup_error.value()
                   << ' ' << std::quoted(error.cleanup_error.message())
                   << " staging_leftover=" << std::quoted(error.staging_leftover) << '\n';
        }
    }
}
