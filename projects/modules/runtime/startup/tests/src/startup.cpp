#include <cheryl/modules/startup.h>
#include <core/controls/input-interface.h>
#include <internals/exceptions.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace {
    struct FactoryCalled {
        std::size_t workers;
    };

    class BorrowedInput final : public CE::Input::System {
        CE::Input::Actions::Bindings bindings_;

    public:
        int initializations = 0;
        void initialize(CE::Platform::Window&) override { ++initializations; }
        void poll() override {}
        void deinitialize() override {}
        CE::Input::Actions::Bindings& bindings() override { return bindings_; }
        CE::Input::DeviceId keyboard_id() const override { return 1; }
        CE::Input::DeviceId mouse_id() const override { return 2; }
        CE::Input::DeviceId gamepad_id() const override { return 3; }
    };
}

TEST(startup, supplied_backend) {
    bool registered = false;
    bool validated = false;
    int backend_value = 0;
    int application_value = 0;
    CE::Application::Startup startup(
        {.arguments =
             [&](CLI::App& options) {
                 registered = true;
                 options.add_option("--backend-value", backend_value);
             },
         .validate = [&] { validated = true; },
         .create = [](const CE::Runtime::ExecutionOptions& options) -> std::unique_ptr<CE::Runtime::Context> {
             throw FactoryCalled{options.worker_count};
         }}
    );
    EXPECT_TRUE(registered);
    startup.add_application_options([&](CLI::App& options) { options.add_option("--application-value", application_value); });
    const char* arguments[]{"startup-test", "--worker-count", "3", "--backend-value", "7", "--application-value", "9"};
    try {
        static_cast<void>(startup.initialize(7, arguments));
        FAIL() << "The supplied factory must be invoked";
    } catch (const FactoryCalled& call) {
        EXPECT_EQ(call.workers, 3u);
    }
    EXPECT_TRUE(validated);
    EXPECT_EQ(backend_value, 7);
    EXPECT_EQ(application_value, 9);
    EXPECT_THROW(static_cast<void>(startup.initialize(7, arguments)), CE::Exceptions::failed_operation);
}

TEST(startup, help) {
    bool created = false;
    CE::Application::Startup startup({.create = [&](const CE::Runtime::ExecutionOptions&) -> std::unique_ptr<CE::Runtime::Context> {
        created = true;
        return nullptr;
    }});
    const char* arguments[]{"startup-test", "--help"};
    const auto result = startup.initialize(2, arguments);
    EXPECT_FALSE(result.should_start());
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(created);
}

TEST(startup, validation) {
    bool created = false;
    CE::Application::Startup startup({.create = [&](const CE::Runtime::ExecutionOptions&) -> std::unique_ptr<CE::Runtime::Context> {
        created = true;
        return nullptr;
    }});
    const char* arguments[]{"startup-test", "--worker-count", "0"};
    const auto result = startup.initialize(3, arguments);
    EXPECT_FALSE(result.should_start());
    EXPECT_NE(result.exit_code, 0);
    EXPECT_FALSE(created);
}

TEST(startup, arguments) {
    CE::Application::Startup startup({.create = [](const CE::Runtime::ExecutionOptions&) -> std::unique_ptr<CE::Runtime::Context> {
        return nullptr;
    }});
    const char* arguments[]{"startup-test", nullptr};
    EXPECT_THROW(static_cast<void>(startup.initialize(2, arguments)), CE::Exceptions::invalid_args);
    EXPECT_THROW(static_cast<void>(startup.initialize(0, nullptr)), CE::Exceptions::invalid_args);
}

#if CHERYL_HAS_STARTUP_OPENGL
TEST(startup, defaults) {
    auto startup = CE::Application::make_startup("Configured startup");
    const char* arguments[]{"startup-test", "--help"};
    ::testing::internal::CaptureStdout();
    const auto result = startup.initialize(2, arguments);
    const auto help = ::testing::internal::GetCapturedStdout();
    EXPECT_FALSE(result.should_start());
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(help.find("--backend"), std::string::npos);
    EXPECT_NE(help.find("--window-width"), std::string::npos);
    EXPECT_NE(help.find("--worker-count"), std::string::npos);
}

TEST(startup, borrowed_input) {
    BorrowedInput input;
    auto startup = CE::Application::make_startup(input);
    const char* arguments[]{"startup-test", "--help"};
    const auto result = startup.initialize(2, arguments);
    EXPECT_FALSE(result.should_start());
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(input.initializations, 0);
}

TEST(startup, selection) {
    auto startup = CE::Application::make_startup();
    const char* arguments[]{"startup-test", "--backend", "unavailable"};
    const auto result = startup.initialize(3, arguments);
    EXPECT_FALSE(result.should_start());
    EXPECT_NE(result.exit_code, 0);
    EXPECT_THROW(
        static_cast<void>(
            CE::Application::make_startup("Invalid backend", {.backend = static_cast<CE::Application::BackendSelection>(99)})
        ),
        CE::Exceptions::invalid_args
    );
}

TEST(startup, window) {
    auto startup = CE::Application::make_startup("Invalid default", {.width = 0});
    const char* arguments[]{"startup-test"};
    const auto result = startup.initialize(1, arguments);
    EXPECT_FALSE(result.should_start());
    EXPECT_NE(result.exit_code, 0);
}
#else
TEST(startup, no_default) {
    EXPECT_THROW(static_cast<void>(CE::Application::make_startup()), CE::Exceptions::failed_operation);
}
#endif
