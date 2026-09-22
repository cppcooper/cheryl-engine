#include <gtest/gtest.h>

#include <core/controls/input-interface.h>
#include <core/display/window-interface.h>

#include <memory>
#include <stdexcept>

#ifdef GLFW_VERSION_MAJOR
#error The engine input contract must not include GLFW.
#endif

namespace {
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

    class BufferedInput final : public CE::Input::iInputSystem {
    public:
        void initialize(CE::iWindow& window) override { window_ = &window; }
        void poll() override {
            if (!window_)
                throw std::logic_error("Input is not attached");
            bindings_.on_button({keyboard_id(), gainput::KeyA}, false, true);
        }
        void deinitialize() override { window_ = nullptr; }

        [[nodiscard]] CE::Input::InputBindings& bindings() override { return bindings_; }
        [[nodiscard]] gainput::DeviceId keyboard_id() const override { return 1; }
        [[nodiscard]] gainput::DeviceId mouse_id() const override { return 2; }
        [[nodiscard]] gainput::DeviceId gamepad_id() const override { return 3; }

    private:
        CE::iWindow* window_ = nullptr;
        CE::Input::InputBindings bindings_;
    };
}

TEST(input_contract, dispatches_bindings_through_an_alternative_window_adapter) {
    TestWindow window;
    std::unique_ptr<CE::Input::iInputSystem> input = std::make_unique<BufferedInput>();
    input->initialize(window);
    bool pressed = false;
    input->bindings().bind_button({input->keyboard_id(), gainput::KeyA},
                                  [&](bool previous, bool current) { pressed = !previous && current; });
    input->poll();
    EXPECT_TRUE(pressed);
    input->deinitialize();
    EXPECT_THROW(input->poll(), std::logic_error);
}
