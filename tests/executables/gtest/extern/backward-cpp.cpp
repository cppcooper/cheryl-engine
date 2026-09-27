#include <gtest/gtest.h>
#include <backward.hpp>
#include <internals/posh.h>
#include <cstddef>

#if defined(_MSC_VER)
#define CHERYL_TEST_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define CHERYL_TEST_NOINLINE __attribute__((noinline))
#else
#define CHERYL_TEST_NOINLINE
#endif

CHERYL_TEST_NOINLINE std::size_t print_stack_here() {
    backward::StackTrace st;
    st.load_here(6);
    backward::Printer p;
    p.object = true;
    p.address = true;
    // Compare frame names in call order without source snippets repeating them.
    p.reverse = false;
    p.snippet = false;
    p.print(st, stderr);
    return st.size();
}

// Use each result after the call so optimized builds retain the nested frames.
CHERYL_TEST_NOINLINE std::size_t call_print_stack() {
    return print_stack_here() + 1;
}

CHERYL_TEST_NOINLINE std::size_t start_nested_calls() {
    return call_print_stack() + 1;
}

TEST(externlibs, backwardcpp_call_stack) {
    testing::internal::CaptureStderr();
    const auto depth = start_nested_calls();
    const std::string trace = testing::internal::GetCapturedStderr();
    ASSERT_GE(depth, 8u);

#if defined(POSH_OS_LINUX) || defined(POSH_OS_WIN64)
    // The trace should list the nested calls from the innermost helper outward.
    const auto inner = trace.find("print_stack_here");
    const auto middle = trace.find("call_print_stack");
    const auto outer = trace.find("start_nested_calls");
    ASSERT_NE(inner, std::string::npos) << trace;
    ASSERT_NE(middle, std::string::npos) << trace;
    ASSERT_NE(outer, std::string::npos) << trace;
    EXPECT_LT(inner, middle) << trace;
    EXPECT_LT(middle, outer) << trace;
#endif
}

#undef CHERYL_TEST_NOINLINE
