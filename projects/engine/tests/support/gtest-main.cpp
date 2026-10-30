#include <gtest/gtest.h>
#include <core/logging/process-lifetime.h>

extern "C" bool ce_debug_terminal_test_process() noexcept { return true; }

int main(int argc, char** argv) {
    const CE::Logging::ProcessLifetime logging;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
