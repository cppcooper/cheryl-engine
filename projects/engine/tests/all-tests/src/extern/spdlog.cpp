#include <gtest/gtest.h>
#include "../../../logging-tests/backend-access.h"

TEST(externlibs, spdlog_console_streams) {
    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();

    // print messages
    std::string in_str_stdout = "hello world";
    std::string in_str_stderr = "goodbye world";
    CE::Logging::Testing::console_probe(in_str_stdout, in_str_stderr);

    // retrieve messages
    std::string out_str_stdout = testing::internal::GetCapturedStdout();
    std::string out_str_stderr = testing::internal::GetCapturedStderr();

    // retrieved messages automatically have '\n' appended
    out_str_stdout.erase(std::remove(out_str_stdout.begin(), out_str_stdout.end(), '\n'), out_str_stdout.end());
    out_str_stderr.erase(std::remove(out_str_stderr.begin(), out_str_stderr.end(), '\n'), out_str_stderr.end());

    // now we can test
    ASSERT_STREQ(in_str_stdout.c_str(), out_str_stdout.c_str());
    ASSERT_STREQ(in_str_stderr.c_str(), out_str_stderr.c_str());
}
