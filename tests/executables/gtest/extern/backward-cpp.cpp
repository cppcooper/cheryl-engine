#include <gtest/gtest.h>
#include <backward.hpp>
#include <internals/posh.h>

void print_stack_here() {
    backward::TraceResolver tr;
    backward::StackTrace st;
    st.load_here(6);
    ASSERT_GE(st.size(), 6);
    backward::Printer p;
    p.object = true;
    p.address = true;
    p.print(st, stderr);
}

void call_print_stack() {
    print_stack_here();
}

void start_nested_calls() {
    call_print_stack();
}

TEST(externlibs, backwardcpp_call_stack) {
    testing::internal::CaptureStderr();
    start_nested_calls();
    const std::string trace = testing::internal::GetCapturedStderr();

#if defined(POSH_OS_LINUX) || defined(POSH_OS_WIN64)
    // The trace should follow the actual call chain from the innermost helper
    // back toward this test. Source line numbers change when tests are edited.
    const auto inner = trace.find("print_stack_here");
    const auto middle = trace.find("call_print_stack");
    const auto outer = trace.find("start_nested_calls");
    ASSERT_NE(inner, std::string::npos) << trace;
    ASSERT_NE(middle, std::string::npos) << trace;
    ASSERT_NE(outer, std::string::npos) << trace;
    EXPECT_LT(inner, middle);
    EXPECT_LT(middle, outer);
#endif
}
