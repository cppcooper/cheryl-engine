#include <gtest/gtest.h>
#include <core/display/glfw-diagnostics.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stdexcept>

TEST(glfw_diagnostics, callback_owner) {
    const auto saved = glfwSetErrorCallback([](int, const char*) { throw std::runtime_error("host callback"); });
    CE::DisplayDetail::install_glfw_diagnostics();
    const auto engine = glfwSetErrorCallback(nullptr);
    glfwSetErrorCallback(engine);
    ASSERT_NE(engine, nullptr);
    const auto before = CE::DisplayDetail::glfw_diagnostics();
    EXPECT_NO_THROW(engine(GLFW_PLATFORM_ERROR, "private description"));
    const auto after = CE::DisplayDetail::glfw_diagnostics();
    EXPECT_EQ(after.errors, before.errors + 1);
    EXPECT_EQ(after.host_failures, before.host_failures + 1);
    EXPECT_EQ(after.last_code, GLFW_PLATFORM_ERROR);
    ::testing::internal::CaptureStderr();
    CE::DisplayDetail::report_glfw_diagnostics("probe", true);
    const auto diagnostic = ::testing::internal::GetCapturedStderr();
    EXPECT_NE(diagnostic.find("operation=probe outcome=native_error"), std::string::npos);
    EXPECT_NE(diagnostic.find("host_callback_failed"), std::string::npos);
    EXPECT_EQ(diagnostic.find("private description"), std::string::npos);
    CE::DisplayDetail::restore_glfw_diagnostics();
    EXPECT_NE(glfwSetErrorCallback(saved), engine);
}

TEST(glfw_diagnostics, host_replacement) {
    const auto saved = glfwSetErrorCallback(nullptr);
    CE::DisplayDetail::install_glfw_diagnostics();
    const auto host = +[](int, const char*) {};
    glfwSetErrorCallback(host);
    CE::DisplayDetail::restore_glfw_diagnostics();
    EXPECT_EQ(glfwSetErrorCallback(saved), host);
}
