#include <gtest/gtest.h>

#include "gtest-output.h"

#include <iostream>
#include <streambuf>

extern "C" std::FILE* ce_debug_terminal_report_stream(int error) noexcept __attribute__((weak));

namespace CE {
    namespace TerminalTestDetail {
        std::FILE* report_file(std::FILE* current) noexcept {
            if (!ce_debug_terminal_report_stream || (current != stdout && current != stderr))
                return current;
            auto* replacement = ce_debug_terminal_report_stream(current == stderr);
            return replacement ? replacement : current;
        }

        namespace {
            class ReportingBuffer final : public std::streambuf {
                const bool error_;

            public:
                explicit ReportingBuffer(const bool error) : error_(error) {}

            protected:
                std::streamsize xsputn(const char* bytes, const std::streamsize count) override {
                    if (count <= 0)
                        return 0;
                    return static_cast<std::streamsize>(std::fwrite(bytes, 1, static_cast<std::size_t>(count),
                        report_file(error_ ? stderr : stdout)));
                }
                int_type overflow(const int_type value) override {
                    if (traits_type::eq_int_type(value, traits_type::eof()))
                        return sync() == 0 ? traits_type::not_eof(value) : traits_type::eof();
                    const char byte = traits_type::to_char_type(value);
                    return xsputn(&byte, 1) == 1 ? value : traits_type::eof();
                }
                int sync() override { return std::fflush(report_file(error_ ? stderr : stdout)); }
            };

            struct ReportingStream {
                ReportingBuffer buffer;
                std::ostream stream;

                explicit ReportingStream(const bool error) : buffer(error), stream(&buffer) { stream.setf(std::ios::unitbuf); }
            };
        }

        std::ostream& output_stream() {
            if (report_file(stdout) == stdout)
                return std::cout;
            static ReportingStream output(false);
            return output.stream;
        }

        std::ostream& error_stream() {
            if (report_file(stderr) == stderr)
                return std::cerr;
            static ReportingStream output(true);
            return output.stream;
        }

        int vprint(const char* format, std::va_list arguments) {
            return std::vfprintf(report_file(stdout), format, arguments);
        }

        int print(const char* format, ...) {
            std::va_list arguments;
            va_start(arguments, format);
            const int result = vprint(format, arguments);
            va_end(arguments);
            return result;
        }

        int fprint(std::FILE* output, const char* format, ...) {
            std::va_list arguments;
            va_start(arguments, format);
            const int result = std::vfprintf(report_file(output), format, arguments);
            va_end(arguments);
            return result;
        }
    }
}

extern "C" bool ce_debug_terminal_test_reports_routed() noexcept { return true; }
