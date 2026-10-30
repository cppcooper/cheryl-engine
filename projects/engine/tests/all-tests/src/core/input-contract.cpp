#include <gtest/gtest.h>

#include <core/controls/input-interface.h>
#include <core/display/window-interface.h>
#include <internals/exceptions.h>

#include <memory>
#include <stdexcept>

#ifdef GLFW_VERSION_MAJOR
#error The engine input contract must not include GLFW.
#endif

namespace {
    constexpr CE::Input::ControlId test_button = 65;

    class TestWindow final : public CE::Platform::Window {
    public:
        [[nodiscard]] CE::Platform::Dimensions<int> logical_size() const override { return {640, 480}; }
        [[nodiscard]] CE::Platform::FramebufferSize framebuffer_size() const override { return {640, 480}; }
        [[nodiscard]] CE::Platform::WindowMode mode() const override { return CE::Platform::WindowMode::NORMAL; }
        [[nodiscard]] bool should_close() const override { return false; }
        void resize(int, int) override {}
        void set_mode(CE::Platform::WindowMode) override {}
        void hide_cursor(bool) const override {}
    };

    /** Emits a button transition while attached; rejects polls after detachment. */
    class BufferedInput final : public CE::Input::System {
        CE::Platform::Window* window_ = nullptr;
        CE::Input::Actions::Bindings bindings_;

    public:
        bool advertises_events = false;

        [[nodiscard]] bool supports(CE::Input::Capture::Channel mode) const override {
            return mode == CE::Input::Capture::Channel::State || (advertises_events && mode == CE::Input::Capture::Channel::Events);
        }

        void initialize(CE::Platform::Window& window) override { window_ = &window; }

        void poll() override {
            if (!window_)
                throw std::logic_error("Input is not attached");
            bindings_.on_button({keyboard_id(), test_button}, true);
            (void)bindings_.publish_actions();
        }

        void deinitialize() override { window_ = nullptr; }

        [[nodiscard]] CE::Input::Actions::Bindings& bindings() override { return bindings_; }
        [[nodiscard]] CE::Input::DeviceId keyboard_id() const override { return 1; }
        [[nodiscard]] CE::Input::DeviceId mouse_id() const override { return 2; }
        [[nodiscard]] CE::Input::DeviceId gamepad_id() const override { return 3; }
    };
} // namespace

TEST(input_contract, alternate_window) {
    // Attach the in-memory input system to an interface-only window and bind
    // one keyboard event before polling the adapter.
    TestWindow window;
    std::unique_ptr<CE::Input::System> input = std::make_unique<BufferedInput>();
    input->initialize(window);
    constexpr CE::Input::Actions::Id action{1};
    (void)input->bindings().bind_button({input->keyboard_id(), test_button}, action);
    input->poll();
    const auto published = input->action_snapshot();
    EXPECT_TRUE(published->button(action).pressed());

    // Once detached, polling must fail instead of delivering another event.
    input->deinitialize();
    EXPECT_THROW(input->poll(), std::logic_error);
}

TEST(input_contract, unsupported_capabilities) {
    BufferedInput input;
    auto state = input.capture(CE::Input::Capture::Channel::State);
    EXPECT_THROW((void)input.capture(CE::Input::Capture::Channel::Events), CE::Exceptions::failed_operation);
    EXPECT_THROW((void)input.capture(CE::Input::Capture::Channel::Text), CE::Exceptions::failed_operation);
    EXPECT_THROW((void)input.routing(), CE::Exceptions::failed_operation);
}

TEST(input_contract, missing_event_support) {
    TestWindow window;
    BufferedInput input;
    input.advertises_events = true;
    input.initialize(window);
    input.poll();
    EXPECT_THROW((void)input.poll_snapshot(), CE::Exceptions::failed_operation);
}
