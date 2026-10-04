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
    constexpr CE::Input::DeviceButtonId test_button = 65;

    class TestWindow final : public CE::iWindow {
    public:
        [[nodiscard]] CE::ViewPort<int> logical_size() const override { return {640, 480}; }
        [[nodiscard]] CE::FramebufferSize framebuffer_size() const override { return {640, 480}; }
        [[nodiscard]] CE::Enum::window_mode mode() const override { return CE::Enum::window_mode::NORMAL; }
        [[nodiscard]] bool should_close() const override { return false; }
        void resize(int, int) override {}
        void set_mode(CE::Enum::window_mode) override {}
        void hide_cursor(bool) const override {}
    };

    /** Emits a button transition while attached; rejects polls after detachment. */
    class BufferedInput final : public CE::Input::iInputSystem {
        CE::iWindow* window_ = nullptr;
        CE::Input::InputBindings bindings_;

    public:
        bool advertises_events = false;

        [[nodiscard]] bool supports(CE::Input::InputMode mode) const override {
            return mode == CE::Input::InputMode::State || (advertises_events && mode == CE::Input::InputMode::Events);
        }

        void initialize(CE::iWindow& window) override { window_ = &window; }

        void poll() override {
            if (!window_)
                throw std::logic_error("Input is not attached");
            bindings_.on_button({keyboard_id(), test_button}, true);
            (void)bindings_.publish_actions();
        }

        void deinitialize() override { window_ = nullptr; }

        [[nodiscard]] CE::Input::InputBindings& bindings() override { return bindings_; }
        [[nodiscard]] CE::Input::DeviceId keyboard_id() const override { return 1; }
        [[nodiscard]] CE::Input::DeviceId mouse_id() const override { return 2; }
        [[nodiscard]] CE::Input::DeviceId gamepad_id() const override { return 3; }
    };
} // namespace

TEST(input_contract, alternate_window) {
    // Attach the in-memory input system to an interface-only window and bind
    // one keyboard event before polling the adapter.
    TestWindow window;
    std::unique_ptr<CE::Input::iInputSystem> input = std::make_unique<BufferedInput>();
    input->initialize(window);
    constexpr CE::Input::ActionId action{1};
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
    auto state = input.capture(CE::Input::InputMode::State);
    EXPECT_THROW((void)input.capture(CE::Input::InputMode::Events), CE::Exceptions::failed_operation);
    EXPECT_THROW((void)input.capture(CE::Input::InputMode::Text), CE::Exceptions::failed_operation);
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
