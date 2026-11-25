#pragma once

#include <cheryl/core/controls/input-record.h>
#include <cheryl/tools/console/model.h>
#include <tools/console/fixture.h>
#include <tools/scenarios/command.h>
#include <array>
#include <functional>

namespace DemoConsole {
    namespace Core = CE::Tools::Console;
    namespace Contact = CE::LivingWorld::Console;
    namespace Scenarios = CE::LivingWorld::Scenarios;
    namespace Presentation = Core::Presentation;
    using Presentation::Entry;
    using Presentation::escaped;
    using Presentation::display_text;
    using Presentation::stage_name;
    using Presentation::code_name;
    class Model;
    class Application final {
        struct State;
        std::unique_ptr<State> state_;

    public:
        explicit Application(Scenarios::Config config);
        ~Application();
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        Contact::Practice& practice() noexcept;
        const Scenarios::Config& config() const noexcept;
        Model& model() noexcept;
        bool quit_requested() const noexcept;
        void stop() noexcept;
        void finish();
    };

    class Model final : public Presentation::Model {
    public:
        explicit Model(Contact::Practice& practice, Scenarios::Config config = {}, std::function<void()> quit = {});
    };

    class Input final {
        static constexpr std::array keys_ = {
            CE::Input::KeyboardKey::W, CE::Input::KeyboardKey::A, CE::Input::KeyboardKey::S,
            CE::Input::KeyboardKey::D, CE::Input::KeyboardKey::Q, CE::Input::KeyboardKey::R,
            CE::Input::KeyboardKey::P, CE::Input::KeyboardKey::Space, CE::Input::KeyboardKey::F5,
            CE::Input::KeyboardKey::F7
        };
        std::array<bool, keys_.size()> down_{}, quarantined_{};
        bool blocked_ = false;

    public:
        void begin(bool keyboard_owned) noexcept;
        void observe(const CE::Input::Events::Record& record) noexcept;
        void suppress() noexcept;
        bool gameplay_allowed() const noexcept { return !blocked_; }
    };

}
