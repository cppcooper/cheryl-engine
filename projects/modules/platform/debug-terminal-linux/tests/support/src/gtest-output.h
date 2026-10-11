#pragma once

#include <cstdarg>
#include <cstdio>
#include <iosfwd>

namespace CE {
    namespace TerminalTestDetail {
        __attribute__((visibility("default"))) std::FILE* report_file(std::FILE* current) noexcept;
        __attribute__((visibility("default"))) std::ostream& output_stream();
        __attribute__((visibility("default"))) std::ostream& error_stream();
        __attribute__((visibility("default"), format(printf, 1, 2))) int print(const char* format, ...);
        __attribute__((visibility("default"))) int vprint(const char* format, std::va_list arguments);
        __attribute__((visibility("default"), format(printf, 2, 3))) int fprint(std::FILE* output, const char* format, ...);
    }
}
