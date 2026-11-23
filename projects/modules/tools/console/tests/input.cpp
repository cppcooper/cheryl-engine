#include <cheryl/tools/console/input.h>
#include <cheryl/tools/console/model.h>
#include <cheryl/tools/console/view.h>
#include <text/font-service.h>
#include <ui/rmlui/session.h>

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <gtest/gtest.h>

#include <initializer_list>
#include <exception>
#include <stdexcept>
#include <thread>

namespace CE::Tools::Console {
    namespace {
        using namespace CE::Input;
        constexpr auto press = Events::ButtonPhase::Press;
        constexpr auto release = Events::ButtonPhase::Release;

        class Source final : public CE::Input::System {
            Actions::Bindings bindings_;
        public:
            void initialize(CE::Platform::Window&) override {}
            void poll() override {}
            void deinitialize() override {}
            Actions::Bindings& bindings() override { return bindings_; }
            DeviceId keyboard_id() const override { return 1; }
            DeviceId mouse_id() const override { return 2; }
            DeviceId gamepad_id() const override { return 3; }
            bool supports(Capture::Channel) const override { return true; }
            bool supports_focus() const override { return true; }
            auto emit(std::initializer_list<Events::Payload> payloads) {
                begin_input_poll();
                for (const auto& payload : payloads) {
                    const auto* button = std::get_if<Events::Button>(&payload);
                    const bool mouse = std::holds_alternative<Events::Pointer>(payload) ||
                        (button && button->mouse_button != MouseButton::Unknown);
                    capture_buffer().record(mouse ? mouse_id() : keyboard_id(),
                        mouse ? DeviceKind::Mouse : DeviceKind::Keyboard, payload);
                }
                return publish_input();
            }
        };

        Events::Button key(KeyboardKey identity, Events::ButtonPhase phase = press,
                           Modifiers modifiers = Modifiers::None) {
            return {control_id(identity), phase, modifiers, -1, -1, identity};
        }

        class Connection final : public Presentation::Connection {
            Authority authority_;
            Registry registry_;
            Context context_ = authority_.issue(0);
            CE::Tools::Console::Session session_{context_};
        public:
            Registry& registry() noexcept override { return registry_; }
            Context context() const noexcept override { return context_; }
            CE::Tools::Console::Session& session() noexcept override { return session_; }
            std::string scope() const override { return "input regression"; }
            void close() noexcept override { session_.close(); }
        };

        struct Fixture {
            Source source;
            CE::Text::FontService fonts;
            CE::UI::RmlUi::Session session{source, fonts, 7};
            Presentation::Model model{std::make_shared<Connection>()};
            Presentation::View view{session, model};
            Presentation::Input input;

            explicit Fixture(Presentation::InputOptions options = {}) : input(session, view, options) {
                session.set_view({1000, 800}, {1000, 800});
            }
            ~Fixture() { input.retire(); }
            bool send(std::initializer_list<Events::Payload> payloads) {
                return input.handle_input(source.emit(payloads)->records);
            }
            auto& field() {
                return dynamic_cast<Rml::ElementFormControlInput&>(
                    *session.context().GetDocument(0)->GetElementById("console-command"));
            }
        };
    }

    TEST(ConsoleInput, DefaultShiftRepeatReleaseAndUnrelatedText) {
        Fixture fixture;
        EXPECT_FALSE(fixture.view.visible());
        EXPECT_TRUE(fixture.send({}));
        EXPECT_FALSE(fixture.send({key(KeyboardKey::GraveAccent)}));
        EXPECT_TRUE(fixture.view.visible());
        EXPECT_TRUE(fixture.session.owns_keyboard_focus());
        EXPECT_FALSE(fixture.send({key(KeyboardKey::GraveAccent, Events::ButtonPhase::Repeat),
            Events::Text{U'~', KeyboardKey::GraveAccent}, key(KeyboardKey::GraveAccent, release)}));
        EXPECT_TRUE(fixture.view.visible());
        EXPECT_TRUE(fixture.field().GetValue().empty());
        EXPECT_FALSE(fixture.send({Events::Text{U'`'}, Events::Text{U'~'}, Events::Text{U'x'}}));
        EXPECT_EQ(fixture.field().GetValue(), "`~x");
        EXPECT_FALSE(fixture.send({key(KeyboardKey::GraveAccent, press, Modifiers::Shift),
            Events::Text{U'~', KeyboardKey::GraveAccent}}));
        EXPECT_FALSE(fixture.view.visible());
        EXPECT_FALSE(fixture.session.owns_keyboard_focus());
        EXPECT_TRUE(fixture.send({}));
        EXPECT_FALSE(fixture.send({key(KeyboardKey::GraveAccent, press, Modifiers::Shift)}));
        EXPECT_TRUE(fixture.view.visible());
    }

    TEST(ConsoleInput, TransitionVetoesWholeBatchAndQuarantinesHeldControls) {
        Fixture fixture;
        EXPECT_TRUE(fixture.send({key(KeyboardKey::W)}));
        const auto batch = fixture.source.emit({Events::Pointer{10, 10}, key(KeyboardKey::A),
            key(KeyboardKey::GraveAccent), key(KeyboardKey::D), Events::Pointer{20, 20}});
        EXPECT_FALSE(fixture.input.handle_input(batch->records, true, [&](auto filtered) {
            ASSERT_EQ(filtered.size(), batch->records.size());
            for (const auto& record : filtered) {
                EXPECT_FALSE(record.to_gameplay);
                if (record.device_kind == DeviceKind::Mouse)
                    EXPECT_TRUE(record.ui_consumed);
                static_cast<void>(fixture.input.route_toggle(record));
            }
            EXPECT_FALSE(fixture.session.owns_keyboard_focus());
            fixture.session.handle_input(filtered, false);
        }));
        EXPECT_TRUE(fixture.view.visible());
        EXPECT_FALSE(fixture.send({key(KeyboardKey::GraveAccent)}));
        EXPECT_FALSE(fixture.send({key(KeyboardKey::W, release)}));
        EXPECT_TRUE(fixture.send({key(KeyboardKey::W), key(KeyboardKey::W, release)}));
    }

    TEST(ConsoleInput, OrdinaryDrainQuarantinesButtonsOnBothSidesOfTransition) {
        Fixture fixture;
        const Events::Button down{control_id(MouseButton::Left), press, Modifiers::None,
            -1, -1, KeyboardKey::Unknown, MouseButton::Left, Events::Pointer{10, 10}};
        auto up = down;
        up.phase = release;
        EXPECT_TRUE(fixture.send({key(KeyboardKey::W)}));
        EXPECT_FALSE(fixture.send({down, key(KeyboardKey::A), key(KeyboardKey::GraveAccent),
            key(KeyboardKey::D), Events::Pointer{20, 20}}));
        EXPECT_FALSE(fixture.send({key(KeyboardKey::GraveAccent), up,
            key(KeyboardKey::W, release), key(KeyboardKey::A, release), key(KeyboardKey::D, release)}));
        EXPECT_TRUE(fixture.send({down, up, key(KeyboardKey::W), key(KeyboardKey::W, release)}));
    }

    TEST(ConsoleInput, OverrideDisablePreemptionAndRetirement) {
        {
            Fixture fixture({KeyboardKey::F10});
            EXPECT_TRUE(fixture.send({key(KeyboardKey::GraveAccent), key(KeyboardKey::GraveAccent, release)}));
            EXPECT_FALSE(fixture.view.visible());
            EXPECT_FALSE(fixture.send({key(KeyboardKey::F10)}));
            EXPECT_TRUE(fixture.view.visible());
            auto successor = fixture.source.routing().focus(99);
            EXPECT_FALSE(fixture.send({Events::Text{U'x'}}));
            EXPECT_FALSE(fixture.view.visible());
            EXPECT_TRUE(successor.owns_focus());
            successor.reset();
            EXPECT_TRUE(fixture.send({}));
            fixture.input.retire();
            EXPECT_NO_THROW(fixture.input.retire());
            EXPECT_THROW(fixture.send({}), std::logic_error);
        }
        {
            Fixture fixture({std::nullopt});
            EXPECT_TRUE(fixture.send({key(KeyboardKey::GraveAccent), key(KeyboardKey::GraveAccent, release)}));
            EXPECT_FALSE(fixture.view.visible());
            fixture.view.open();
            EXPECT_FALSE(fixture.send({}));
            EXPECT_FALSE(fixture.send({Events::Text{U'`', KeyboardKey::GraveAccent}}));
            EXPECT_EQ(fixture.field().GetValue(), "`");
        }
    }

    TEST(ConsoleInput, MultipleTogglesAndUndrawableDoNotAcquireFocus) {
        Fixture fixture;
        EXPECT_FALSE(fixture.send({key(KeyboardKey::GraveAccent), key(KeyboardKey::GraveAccent)}));
        EXPECT_FALSE(fixture.view.visible());
        EXPECT_FALSE(fixture.input.handle_input(fixture.source.emit({key(KeyboardKey::GraveAccent)})->records, false));
        EXPECT_FALSE(fixture.view.visible());
        EXPECT_TRUE(fixture.send({}));
    }

    TEST(ConsoleInput, OwnerRetirementIsSafeToRepeatAfterJoin) {
        std::unique_ptr<Fixture> retired;
        std::exception_ptr failure;
        std::thread owner([&] {
            try {
                auto fixture = std::make_unique<Fixture>();
                EXPECT_FALSE(fixture->send({key(KeyboardKey::GraveAccent)}));
                EXPECT_TRUE(fixture->view.visible());
                fixture->input.retire();
                EXPECT_FALSE(fixture->view.visible());
                EXPECT_FALSE(fixture->session.owns_keyboard_focus());
                retired = std::move(fixture);
            } catch (...) {
                failure = std::current_exception();
            }
        });
        owner.join();
        if (failure)
            std::rethrow_exception(failure);
        ASSERT_NE(retired, nullptr);
        EXPECT_NO_THROW(retired->input.retire());
        EXPECT_THROW(retired->send({}), std::logic_error);
        EXPECT_NO_THROW(retired.reset());
    }
}
