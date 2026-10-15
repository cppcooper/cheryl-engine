#include <gtest/gtest.h>

#include <core/display/display-system-interface.h>
#include <core/rendering/renderer.h>

#include <memory>
#include <stdexcept>

#ifdef GLFW_VERSION_MAJOR
#error The renderer display contract must not include GLFW.
#endif

namespace {
    /** Keeps resize, mode, cursor, and close state behind the window interface. */
    class MemoryWindow final : public CE::Platform::Window {
    public:
        MemoryWindow(int width, int height)
        : logical_size_(width, height), framebuffer_size_{width, height} {}

        [[nodiscard]] CE::Platform::Dimensions<int> logical_size() const override { return logical_size_; }
        [[nodiscard]] CE::Platform::FramebufferSize framebuffer_size() const override { return framebuffer_size_; }
        [[nodiscard]] CE::Platform::WindowMode mode() const override { return mode_; }
        [[nodiscard]] bool should_close() const override { return closed_; }

        void resize(int width, int height) override {
            logical_size_ = {width, height};
            framebuffer_size_ = {width, height};
        }

        void set_mode(CE::Platform::WindowMode mode) override { mode_ = mode; }
        void hide_cursor(bool hide) const override { hidden_ = hide; }

        void request_close() { closed_ = true; }
        [[nodiscard]] bool cursor_hidden() const { return hidden_; }

    private:
        CE::Platform::Dimensions<int> logical_size_;
        CE::Platform::FramebufferSize framebuffer_size_;
        CE::Platform::WindowMode mode_ = CE::Platform::WindowMode::NORMAL;
        bool closed_ = false;
        mutable bool hidden_ = false;
    };

    /** Validates window activation while exposing it through iDisplaySystem. */
    class MemoryDisplay final : public CE::Platform::DisplaySystem {
    public:
        [[nodiscard]] const std::vector<CE::Platform::Monitor>& monitors() const override { return monitors_; }
        [[nodiscard]] int monitor_count() const override { return static_cast<int>(monitors_.size()); }
        [[nodiscard]] const CE::Platform::Monitor& primary_monitor() const override { return monitors_.front(); }
        [[nodiscard]] CE::Platform::Window* active_window() const override { return active_; }
        [[nodiscard]] std::pair<float, float> content_scale(const CE::Platform::Monitor&) const override { return {1, 1}; }

        CE::Platform::Window*
        create_window(const CE::Platform::Monitor& monitor, CE::Platform::WindowMode mode, int width, int height) override {
            if (monitor.id() != primary_monitor().id())
                throw std::invalid_argument("Unknown monitor");
            window_ = std::make_unique<MemoryWindow>(width, height);
            window_->set_mode(mode);
            return window_.get();
        }

        void activate_window(CE::Platform::Window& window) override {
            if (&window != window_.get())
                throw std::invalid_argument("Unknown window");
            active_ = &window;
        }

        [[nodiscard]] MemoryWindow& window() { return *window_; }

    private:
        std::vector<CE::Platform::Monitor> monitors_{{1, 1920, 1080}};
        std::unique_ptr<MemoryWindow> window_;
        CE::Platform::Window* active_ = nullptr;
    };
}

TEST(display_contract, portable_display) {
    // Create and activate a window through interface pointers backed by
    // in-memory implementations, with no native display dependency.
    auto backend = std::make_unique<MemoryDisplay>();
    std::unique_ptr<CE::Platform::DisplaySystem> display = std::move(backend);
    ASSERT_EQ(display->monitor_count(), 1);
    auto* window = display->create_window(display->primary_monitor(), CE::Platform::WindowMode::NORMAL, 640, 480);
    display->activate_window(*window);
    EXPECT_EQ(display->active_window(), window);

    // Change window size, mode, and cursor state through the abstraction;
    // read back both interface state and the test backend's recorded state.
    window->resize(800, 600);
    window->set_mode(CE::Platform::WindowMode::BORDERLESS);
    window->hide_cursor(true);
    EXPECT_EQ(window->framebuffer_size(), (CE::Platform::FramebufferSize{800, 600}));
    EXPECT_EQ(window->mode(), CE::Platform::WindowMode::BORDERLESS);
    EXPECT_FALSE(window->should_close());

    auto* memory_display = static_cast<MemoryDisplay*>(display.get());
    EXPECT_TRUE(memory_display->window().cursor_hidden());

    // The close request and resize event must refer to the active window
    // without exposing any platform-specific window handle.
    memory_display->window().request_close();
    EXPECT_TRUE(display->active_window()->should_close());
    const CE::Platform::WindowResized resized{window, window->framebuffer_size()};
    EXPECT_EQ(resized.window, display->active_window());
    EXPECT_EQ(resized.size, (CE::Platform::FramebufferSize{800, 600}));
}
