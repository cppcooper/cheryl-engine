#pragma once

#include "developer-console-model.h"

#include <core/controls/input-interface.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <initializer_list>
#include <stdexcept>
#include <thread>

namespace DemoConsoleTests {
    namespace Core = DemoConsole::Core;
    namespace Contact = DemoConsole::Contact;
    namespace Scenarios = DemoConsole::Scenarios;

    inline CE::Input::Events::Button native_key(
        CE::Input::KeyboardKey selected, CE::Input::Events::ButtonPhase phase = CE::Input::Events::ButtonPhase::Press,
        CE::Input::Modifiers modifiers = CE::Input::Modifiers::None
    ) {
        return {800, phase, modifiers, -1, -1, selected};
    }
    inline CE::Input::Events::Button native_left(CE::Input::Events::Pointer position, CE::Input::Events::ButtonPhase phase) {
        return {801, phase, CE::Input::Modifiers::None, -1, -1, CE::Input::KeyboardKey::Unknown, CE::Input::MouseButton::Left, position};
    }
    class NativeInputSource final : public CE::Input::System {
        CE::Input::Actions::Bindings bindings_;

    public:
        void initialize(CE::Platform::Window&) override {}
        void poll() override {}
        void deinitialize() override {}
        CE::Input::Actions::Bindings& bindings() override { return bindings_; }
        CE::Input::DeviceId keyboard_id() const override { return 1; }
        CE::Input::DeviceId mouse_id() const override { return 2; }
        CE::Input::DeviceId gamepad_id() const override { return 3; }
        bool supports(CE::Input::Capture::Channel) const override { return true; }
        bool supports_focus() const override { return true; }
        std::shared_ptr<const CE::Input::PollSnapshot> emit(std::initializer_list<CE::Input::Events::Payload> records) {
            using namespace CE::Input;
            begin_input_poll();
            for (const auto& record : records) {
                const auto* button = std::get_if<Events::Button>(&record);
                const bool mouse = std::holds_alternative<Events::Pointer>(record) || std::holds_alternative<Events::Scroll>(record) ||
                                   (button && button->mouse_button != MouseButton::Unknown);
                capture_buffer().record(mouse ? mouse_id() : keyboard_id(), mouse ? DeviceKind::Mouse : DeviceKind::Keyboard, record);
            }
            return publish_input();
        }
    };

    inline const Core::Value& field(const Core::Result& result, std::size_t index) {
        return std::get<Core::Value::Children>(result.value.data).at(index);
    }
    inline std::uint64_t number(const Core::Result& result, std::size_t index) {
        return std::get<std::uint64_t>(field(result, index).data);
    }
    inline std::string source(const Contact::Practice& practice, unsigned quantity) {
        std::string result{practice.plan->source()};
        const auto offset = result.find("12 hp");
        if (offset == std::string::npos)
            throw std::runtime_error("S1 source changed");
        result.replace(offset, 5, std::to_string(quantity) + " hp");
        std::replace(result.begin(), result.end(), '\n', ' ');
        return result;
    }
    class Files final {
        static inline std::atomic<unsigned> sequence = 0;

    public:
        CE::Workers::Pool pool{1};
        std::shared_ptr<Scenarios::IoService> io = std::make_shared<Scenarios::IoService>(pool.make_group());
        std::filesystem::path root;

        Files() {
            root = std::filesystem::temp_directory_path() /
                ("cheryl-demo-console-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                 std::to_string(++sequence));
            if (!std::filesystem::create_directory(root))
                throw std::runtime_error("Cannot create console test output root");
        }
        ~Files() {
            io->settle();
            std::error_code ignored;
            std::filesystem::remove_all(root, ignored);
        }
        Scenarios::Config config() const {
            Scenarios::Config value;
            value.script_root = DEMO_CONSOLE_SCRIPT_ROOT;
            value.asset_root = DEMO_CONSOLE_MAGIC_ROOT;
            value.output_root = root;
            value.build_id = "unexecuted-demo-regression-source";
            value.io = io;
            return value;
        }
    };
    inline void settle(DemoConsole::Model& model) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!Core::terminal(model.entries().back().result.stage) && std::chrono::steady_clock::now() < deadline) {
            model.pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}
