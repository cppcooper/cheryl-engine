#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>

TEST(terminal_report, pass) {
    std::puts("application-output");
    std::fputs("application-error\n", stderr);
    EXPECT_EQ(1, 1);
}

TEST(terminal_report, failure) { ADD_FAILURE() << "framework-assertion"; }

TEST(terminal_report, diagnostic) { GTEST_LOG_(WARNING) << "framework-diagnostic"; }

TEST(terminal_report, capture) {
    ::testing::internal::CaptureStdout();
    std::puts("captured-output");
    const auto output = ::testing::internal::GetCapturedStdout();
    ::testing::internal::CaptureStderr();
    std::fputs("captured-error\n", stderr);
    const auto error = ::testing::internal::GetCapturedStderr();
    EXPECT_EQ(output, "captured-output\n");
    EXPECT_EQ(error, "captured-error\n");
}

TEST(terminal_report, death) {
    EXPECT_EXIT(
        {
            std::fputs("death-record\n", stderr);
            static_cast<void>(std::fflush(stderr));
            std::_Exit(7);
        },
        ::testing::ExitedWithCode(7), "death-record"
    );
}
