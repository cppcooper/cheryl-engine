#include <gtest/gtest.h>
#include <backward.hpp>
#include <regex>
#include <internals/posh.h>

void foo3ty() {
    backward::TraceResolver tr;
    backward::StackTrace st;
    st.load_here(6);
    ASSERT_GE(st.size(), 6);
    backward::Printer p;
    p.object = true;
    p.address = true;
    p.print(st, stderr);
}

void foo2toot() {
    return foo3ty();
}

void foo1UR() {
    return foo2toot();
}

TEST(externlibs, backwardcpp) {
    testing::internal::CaptureStderr();
    foo1UR();
    std::string out_str_stderr = testing::internal::GetCapturedStderr();
    //std::cout<<out_str_stderr<<std::endl;
#ifdef POSH_OS_LINUX
    std::regex frame4(R"(#4.*foo1UR.*\n.*tests\/automated\/externlibs\/backward-cpp.cpp.*line 22.*foo1UR)");
    std::regex frame3(R"(#3.*foo2toot.*\n.*tests\/automated\/externlibs\/backward-cpp.cpp.*line 18.*foo2toot)");
    std::regex frame2(R"(#2.*foo3ty.*\n.*tests\/automated\/externlibs\/backward-cpp.cpp.*line 9.*foo3ty)");
    ASSERT_TRUE(std::regex_search(out_str_stderr, frame4));
    ASSERT_TRUE(std::regex_search(out_str_stderr, frame3));
    ASSERT_TRUE(std::regex_search(out_str_stderr, frame2));
#elif defined POSH_OS_WIN64
    std::regex frame3(R"(#3.*foo1UR\n.*tests\\executables\\gtest\\extern\\backward-cpp.cpp.*line 22.*foo1UR)");
    std::regex frame2(R"(#2.*foo2toot\n.*tests\\executables\\gtest\\extern\\backward-cpp.cpp.*line 18.*foo2toot)");
    std::regex frame1(R"(#1.*foo3ty\n.*tests\\executables\\gtest\\extern\\backward-cpp.cpp.*line 9.*foo3ty)");
    ASSERT_TRUE(std::regex_search(out_str_stderr, frame3));
    ASSERT_TRUE(std::regex_search(out_str_stderr, frame2));
    ASSERT_TRUE(std::regex_search(out_str_stderr, frame1));
#endif
}
