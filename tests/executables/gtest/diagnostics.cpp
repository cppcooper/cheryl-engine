#include <internals/bounded-stream.h>
#include <internals/exceptions.h>
#include <internals/failure-reporting.h>
#include <core/logging/log.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>

TEST(diagnostics, bounded_trace) {
    CE::Diagnostics::BoundedStreamBuffer<32> buffer;
    std::ostream stream(&buffer);
    stream << std::string(200, 'x');
    const auto truncated = buffer.view();
    EXPECT_EQ(truncated.size(), 31u);
    EXPECT_TRUE(truncated.ends_with("... [truncated]"));
    EXPECT_EQ(truncated.data()[truncated.size()], '\0');
    buffer.reset();
    stream.clear();
    stream << "second trace";
    EXPECT_EQ(buffer.view(), "second trace");
}

TEST(diagnostics, owned_fallback) {
    std::string information = "primary failure";
    auto error = CE::Exceptions::exception_base::fallback("failed_operation", "probe", 42, information.c_str());
    information.clear();
    EXPECT_NE(std::string_view(error.what()).find("failed_operation at line 42 inside probe"), std::string_view::npos);
    EXPECT_NE(std::string_view(error.what()).find("primary failure"), std::string_view::npos);
    auto copy = error;
    auto moved = std::move(copy);
    EXPECT_STREQ(moved.what(), error.what());
    const std::string long_information(4096, 'x');
    const auto truncated = CE::Exceptions::exception_base::fallback("bad_alloc", "probe", 1, long_information.c_str());
    EXPECT_LE(std::strlen(truncated.what()), 511u);
    EXPECT_EQ(std::strlen(truncated.what()), 511u);
}

TEST(diagnostics, repeated_traces) {
    for (int index = 0; index < 3; ++index) {
        const CE::Exceptions::failed_operation error("probe", index, "primary failure");
        EXPECT_NE(std::string_view(error.what()).find("primary failure"), std::string_view::npos);
        const auto trace = CE::stack_trace();
        EXPECT_LE(trace.size(), 6143u);
    }
}

TEST(diagnostics, fallback_record) {
    const auto file = std::unique_ptr<std::FILE, decltype(&std::fclose)>(std::tmpfile(), std::fclose);
    ASSERT_NE(file, nullptr);
    std::exception_ptr failure;
    try {
        throw std::runtime_error("cleanup failed");
    } catch (...) {
        failure = std::current_exception();
    }
    EXPECT_NO_THROW(CE::Diagnostics::report_failure("game deinit", failure, file.get()));
    std::rewind(file.get());
    std::array<char, 1025> text{};
    const auto count = std::fread(text.data(), 1, text.size() - 1, file.get());
    EXPECT_NE(std::string_view(text.data(), count).find("game deinit: cleanup failed"), std::string_view::npos);
}
