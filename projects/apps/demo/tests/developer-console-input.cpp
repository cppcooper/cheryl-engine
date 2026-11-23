#include "developer-console-fixture.h"
#include "developer-console.h"
#include "developer-console-routing.h"
#include "ui-routing.h"

#include <ui/rmlui/session.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>

#include <initializer_list>

namespace {
    using namespace CE::Input;
    using namespace DemoConsoleTests;

    Events::Record key(KeyboardKey selected, Events::ButtonPhase phase = Events::ButtonPhase::Press) {
        return {1, {}, 1, DeviceKind::Keyboard, Events::Button{800, phase, Modifiers::None, -1, -1, selected}};
    }

    using InputSource = NativeInputSource;

    Events::Button pointer_button(Events::Pointer position, Events::ButtonPhase phase) {
        return {801, phase, Modifiers::None, -1, -1, KeyboardKey::Unknown, MouseButton::Left, position};
    }

    class RoutedConsole final {
    public:
        InputSource input;
        CE::UI::RmlUi::Session session{input, 3};
        Contact::Practice practice = Contact::practice();
        DemoConsole::Model model{practice};
        std::unique_ptr<DemoConsole::View> view;
        Rml::ElementDocument* original = nullptr;
        Rml::ElementDocument* console = nullptr;
        std::unique_ptr<DemoConsole::Routing> routing;
        CE::Platform::Dimensions<int> logical{1000, 800};
        CE::Platform::FramebufferSize framebuffer{1000, 800};

        RoutedConsole() {
            if (!session.load_font(DEMO_CONSOLE_TEST_FONT, "proof"))
                throw std::runtime_error("Cannot load the real console font fixture");
            session.set_view({1000, 800}, {1000, 800});
            session.context().GetRootElement()->SetProperty("font-family", "proof");
            original = session.context().LoadDocumentFromMemory(R"rml(
                <rml><head><style>
                    body { position: absolute; left: 0px; top: 0px; width: 1000px; height: 800px; margin: 0px; }
                    #panel { position: absolute; left: 0px; top: 0px; width: 300px; height: 200px; }
                    #reset { position: absolute; left: 10px; top: 10px; width: 150px; height: 25px; }
                    #field { position: absolute; left: 10px; top: 60px; width: 200px; height: 25px; }
                    #change-image { position: absolute; left: 10px; top: 110px; width: 150px; height: 25px; }
                </style></head><body><div id="panel"><button id="reset">Reset</button>
                <input id="field" type="text" /><button id="change-image">Change image</button><div id="image"></div>
                </div></body></rml>
            )rml");
            if (!original)
                throw std::runtime_error("Cannot load the real original-controls document");
            original->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
            view = std::make_unique<DemoConsole::View>(session, model);
            console = session.context().GetDocument(1);
            routing = std::make_unique<DemoConsole::Routing>(session, *original, view.get());
            session.update_time(0);
        }

        bool route(std::initializer_list<Events::Payload> records, bool drawable = true) {
            const auto batch = input.emit(records);
            const bool allowed = routing->handle_input(batch->records, drawable && DemoUiRouting::drawable(logical, framebuffer));
            view->update();
            session.update_time(0);
            return allowed;
        }

        void set_view(CE::Platform::Dimensions<int> next_logical, CE::Platform::FramebufferSize next_framebuffer) {
            logical = next_logical;
            framebuffer = next_framebuffer;
            session.set_view(logical, framebuffer);
        }

        Events::Pointer point(Rml::Element& element) {
            const auto offset = element.GetAbsoluteOffset();
            return {offset.x + 2, offset.y + 2};
        }

        Events::Pointer reset_point() { return point(*original->GetElementById("reset")); }
        bool reset_effect() { return routing->consume_reset(); }
        void click(Events::Pointer position) {
            route({pointer_button(position, Events::ButtonPhase::Press), pointer_button(position, Events::ButtonPhase::Release)});
        }
    };
}

TEST(DemoConsoleInput, OpeningClosingAndHeldKeysQuarantineTheWholeBatch) {
    DemoConsole::Input input;
    input.begin(false);
    input.observe(key(KeyboardKey::W));
    EXPECT_TRUE(input.gameplay_allowed());
    input.observe(key(KeyboardKey::GraveAccent));
    input.suppress();
    input.observe(key(KeyboardKey::Q));
    input.suppress();
    EXPECT_FALSE(input.gameplay_allowed());
    input.begin(false);
    EXPECT_FALSE(input.gameplay_allowed());
    input.observe(key(KeyboardKey::W, Events::ButtonPhase::Release));
    input.observe(key(KeyboardKey::Q, Events::ButtonPhase::Release));
    EXPECT_FALSE(input.gameplay_allowed());
    input.begin(false);
    EXPECT_TRUE(input.gameplay_allowed());
    input.observe(key(KeyboardKey::D));
    EXPECT_TRUE(input.gameplay_allowed());
}

TEST(DemoConsoleInput, PreemptionAndMinimizeDoNotResumeStaleMovement) {
    DemoConsole::Input input;
    input.begin(true);
    input.observe(key(KeyboardKey::S));
    input.suppress();
    input.begin(false);
    EXPECT_FALSE(input.gameplay_allowed());
    input.observe(key(KeyboardKey::S, Events::ButtonPhase::Repeat));
    input.begin(false);
    EXPECT_FALSE(input.gameplay_allowed());
    input.observe(key(KeyboardKey::S, Events::ButtonPhase::Release));
    input.begin(false);
    EXPECT_TRUE(input.gameplay_allowed());
}

TEST(DemoConsoleInput, OneNativeSessionKeepsEpochAndOwnedUnicodeSubmission) {
    RoutedConsole routed;
    routed.route({key(KeyboardKey::F4).data});
    ASSERT_TRUE(routed.session.owns_keyboard_focus());
    auto committed = routed.input.capture(Capture::Channel::Text);
    EXPECT_FALSE(routed.route({key(KeyboardKey::GraveAccent).data, Events::Text{U'q'}}));
    ASSERT_TRUE(routed.view->visible());
    auto* field = dynamic_cast<Rml::ElementFormControlInput*>(routed.console->GetElementById("console-command"));
    ASSERT_NE(field, nullptr);
    EXPECT_EQ(field->GetValue(), "get practice.heal_price");
    EXPECT_EQ(dynamic_cast<Rml::ElementFormControlInput*>(routed.original->GetElementById("field"))->GetValue(), "");
    field->SetValue({});
    routed.route({Events::Text{0x6c34}, Events::Text{0x1f331}});
    EXPECT_EQ(field->GetValue(), "\xe6\xb0\xb4\xf0\x9f\x8c\xb1");
    field->SetValue("get practice.heal_price");
    routed.route({key(KeyboardKey::Enter).data});
    ASSERT_EQ(routed.model.entries().size(), 1u);
    routed.model.pump();
    EXPECT_EQ(number(routed.model.entries()[0].result, 1), routed.practice.host->price());
    EXPECT_FALSE(routed.route({key(KeyboardKey::Escape).data, key(KeyboardKey::Q).data, Events::Text{U'q'}}));
    EXPECT_FALSE(routed.view->visible());
    EXPECT_FALSE(routed.session.owns_keyboard_focus());
    EXPECT_EQ(routed.model.entries().size(), 1u);
    EXPECT_FALSE(routed.route({}));
    EXPECT_FALSE(routed.route({key(KeyboardKey::Q, Events::ButtonPhase::Release).data}));
    EXPECT_TRUE(routed.route({}));
    routed.model.close();
}

TEST(DemoConsoleInput, SharedDefaultShiftToggleSeparatesAssociatedAndPastedText) {
    RoutedConsole routed;
    auto shifted = std::get<Events::Button>(key(KeyboardKey::GraveAccent).data);
    shifted.modifiers = Modifiers::Shift;
    EXPECT_FALSE(routed.route({shifted}));
    ASSERT_TRUE(routed.view->visible());
    auto* field = dynamic_cast<Rml::ElementFormControlInput*>(routed.console->GetElementById("console-command"));
    ASSERT_NE(field, nullptr);
    field->SetValue({});
    EXPECT_FALSE(routed.route({key(KeyboardKey::GraveAccent, Events::ButtonPhase::Repeat).data,
        Events::Text{U'~', KeyboardKey::GraveAccent}, key(KeyboardKey::GraveAccent, Events::ButtonPhase::Release).data}));
    EXPECT_TRUE(routed.view->visible());
    EXPECT_TRUE(field->GetValue().empty());
    routed.route({Events::Text{U'`'}, Events::Text{U'~'}});
    EXPECT_EQ(field->GetValue(), "`~");
    field->SetValue("get practice.heal_price");
    EXPECT_FALSE(routed.route({key(KeyboardKey::Enter).data, shifted,
        Events::Text{U'~', KeyboardKey::GraveAccent}}));
    EXPECT_FALSE(routed.view->visible());
    EXPECT_TRUE(routed.model.entries().empty());
    EXPECT_FALSE(routed.route({shifted}));
    ASSERT_TRUE(routed.view->visible());
    routed.route({key(KeyboardKey::Enter).data});
    EXPECT_EQ(routed.model.entries().size(), 1u);
}

TEST(DemoConsoleInput, ProductionRouteCancelsResetBeforeAndAfterConsoleToggle) {
    RoutedConsole routed;
    const auto position = routed.reset_point();
    const auto press = pointer_button(position, Events::ButtonPhase::Press);
    const auto release = pointer_button(position, Events::ButtonPhase::Release);
    routed.route({press, release});
    ASSERT_TRUE(routed.reset_effect());
    EXPECT_FALSE(routed.reset_effect());
    EXPECT_FALSE(routed.route({key(KeyboardKey::GraveAccent).data, press, release}));
    EXPECT_TRUE(routed.view->visible());
    EXPECT_FALSE(routed.reset_effect());
    EXPECT_FALSE(routed.route({key(KeyboardKey::GraveAccent).data, press, release}));
    EXPECT_FALSE(routed.view->visible());
    EXPECT_FALSE(routed.reset_effect());
    routed.route({});
    EXPECT_FALSE(routed.reset_effect());
    routed.route({press, release});
    ASSERT_TRUE(routed.reset_effect());
    EXPECT_FALSE(routed.route({press, release, key(KeyboardKey::GraveAccent).data}));
    EXPECT_TRUE(routed.view->visible());
    EXPECT_FALSE(routed.reset_effect());
    routed.route({key(KeyboardKey::Escape).data});
    routed.route({});
    EXPECT_FALSE(routed.reset_effect());
    routed.route({press, release});
    EXPECT_TRUE(routed.reset_effect());
}

TEST(DemoConsoleInput, ProductionCloseOutsideEscapeAndRepeatedTogglesOwnTheBatch) {
    RoutedConsole routed;
    const auto position = routed.reset_point();
    const auto press = pointer_button(position, Events::ButtonPhase::Press);
    const auto release = pointer_button(position, Events::ButtonPhase::Release);
    routed.route({press, release});
    ASSERT_TRUE(routed.reset_effect());
    routed.route({key(KeyboardKey::GraveAccent).data});
    ASSERT_TRUE(routed.view->visible());
    const auto close = routed.point(*routed.console->GetElementById("console-close"));
    auto* hit = routed.session.context().GetElementAtPoint({static_cast<float>(close.x), static_cast<float>(close.y)});
    while (hit && hit->GetId() != "console-close")
        hit = hit->GetParentNode();
    ASSERT_EQ(hit, routed.console->GetElementById("console-close"));
    ASSERT_TRUE(routed.view->inside(close));
    const auto close_activity = routed.view->activity();
    EXPECT_FALSE(routed.route({pointer_button(close, Events::ButtonPhase::Press), pointer_button(close, Events::ButtonPhase::Release), press, release}));
    EXPECT_EQ(routed.view->activity(), close_activity + 2);
    EXPECT_FALSE(routed.view->visible());
    EXPECT_FALSE(routed.reset_effect());
    routed.route({key(KeyboardKey::GraveAccent).data});
    ASSERT_FALSE(routed.view->inside(position));
    EXPECT_FALSE(routed.route({press, release, press, release}));
    EXPECT_FALSE(routed.view->visible());
    EXPECT_FALSE(routed.reset_effect());
    routed.route({key(KeyboardKey::GraveAccent).data});
    EXPECT_FALSE(routed.route({key(KeyboardKey::Escape).data, press, release}));
    EXPECT_FALSE(routed.reset_effect());
    EXPECT_FALSE(routed.route({key(KeyboardKey::GraveAccent).data, key(KeyboardKey::GraveAccent).data, press, release}));
    EXPECT_FALSE(routed.view->visible());
    EXPECT_FALSE(routed.reset_effect());
    EXPECT_FALSE(routed.route({key(KeyboardKey::GraveAccent).data, key(KeyboardKey::GraveAccent).data, key(KeyboardKey::GraveAccent).data, press, release}));
    EXPECT_TRUE(routed.view->visible());
    EXPECT_FALSE(routed.reset_effect());
    routed.route({key(KeyboardKey::Escape).data});
    routed.route({});
    EXPECT_FALSE(routed.reset_effect());
    routed.route({press, release});
    EXPECT_TRUE(routed.reset_effect());
}

TEST(DemoConsoleInput, OriginalPressCannotCompleteAfterAConsoleTransition) {
    RoutedConsole routed;
    const auto position = routed.reset_point();
    const auto press = pointer_button(position, Events::ButtonPhase::Press);
    const auto release = pointer_button(position, Events::ButtonPhase::Release);
    routed.route({press, release});
    ASSERT_TRUE(routed.reset_effect());
    routed.route({press});
    EXPECT_FALSE(routed.reset_effect());
    routed.route({key(KeyboardKey::GraveAccent).data, key(KeyboardKey::Escape).data});
    EXPECT_FALSE(routed.view->visible());
    EXPECT_FALSE(routed.route({release}));
    EXPECT_FALSE(routed.reset_effect());
    routed.route({});
    EXPECT_FALSE(routed.reset_effect());
    routed.route({press, release});
    EXPECT_TRUE(routed.reset_effect());
}

TEST(DemoConsoleInput, OrdinaryFieldResetImageAndVisibilityControlsRemainIntentional) {
    RoutedConsole routed;
    routed.route({key(KeyboardKey::F4).data});
    ASSERT_TRUE(routed.session.owns_keyboard_focus());
    EXPECT_EQ(routed.session.context().GetFocusElement(), routed.original->GetElementById("field"));
    routed.click(routed.reset_point());
    ASSERT_TRUE(routed.reset_effect());
    auto* picture = routed.original->GetElementById("image");
    const auto image = routed.point(*routed.original->GetElementById("change-image"));
    routed.click(image);
    EXPECT_EQ(picture->GetAttribute<Rml::String>("src", ""), "../textures/ui/quadrants-alt.png");
    routed.route({pointer_button(image, Events::ButtonPhase::Press), pointer_button(image, Events::ButtonPhase::Release),
                  key(KeyboardKey::GraveAccent).data});
    EXPECT_TRUE(routed.view->visible());
    EXPECT_EQ(picture->GetAttribute<Rml::String>("src", ""), "../textures/ui/quadrants-alt.png");
    const auto position = routed.reset_point();
    EXPECT_FALSE(routed.route({key(KeyboardKey::F4).data, pointer_button(position, Events::ButtonPhase::Press),
                               pointer_button(position, Events::ButtonPhase::Release)}));
    EXPECT_FALSE(routed.view->visible());
    EXPECT_FALSE(routed.reset_effect());
    EXPECT_EQ(routed.session.context().GetFocusElement(), routed.original->GetElementById("field"));
    routed.click(position);
    EXPECT_TRUE(routed.reset_effect());
    routed.route({key(KeyboardKey::F6).data, pointer_button(position, Events::ButtonPhase::Press),
                  pointer_button(position, Events::ButtonPhase::Release)});
    EXPECT_FALSE(routed.routing->original_visible());
    EXPECT_FALSE(routed.reset_effect());
    routed.route({key(KeyboardKey::F6).data});
    EXPECT_TRUE(routed.routing->original_visible());
    routed.click(position);
    EXPECT_TRUE(routed.reset_effect());
}

TEST(DemoConsoleInput, ProductionPreemptionAndZeroDrawabilityCancelEffectsAndQuarantineHeldKeys) {
    RoutedConsole routed;
    routed.click(routed.reset_point());
    ASSERT_TRUE(routed.reset_effect());
    routed.route({key(KeyboardKey::GraveAccent).data, key(KeyboardKey::W).data});
    ASSERT_TRUE(routed.view->visible());
    auto successor = routed.input.routing().focus(99);
    const auto position = routed.reset_point();
    EXPECT_FALSE(routed.route({pointer_button(position, Events::ButtonPhase::Press), pointer_button(position, Events::ButtonPhase::Release)}));
    EXPECT_FALSE(routed.view->visible());
    EXPECT_FALSE(routed.reset_effect());
    EXPECT_TRUE(successor.owns_focus());
    successor.reset();
    EXPECT_FALSE(routed.route({}));
    EXPECT_FALSE(routed.route({key(KeyboardKey::W, Events::ButtonPhase::Release).data}));
    EXPECT_TRUE(routed.route({}));
    routed.click(position);
    EXPECT_TRUE(routed.reset_effect());
    for (bool zero_logical : {true, false}) {
        routed.route({key(KeyboardKey::GraveAccent).data, key(KeyboardKey::S).data});
        ASSERT_TRUE(routed.view->visible());
        routed.set_view(zero_logical ? CE::Platform::Dimensions<int>{0, 0} : CE::Platform::Dimensions<int>{1000, 800},
                        zero_logical ? CE::Platform::FramebufferSize{1000, 800} : CE::Platform::FramebufferSize{0, 0});
        EXPECT_FALSE(routed.route({pointer_button(position, Events::ButtonPhase::Press), pointer_button(position, Events::ButtonPhase::Release)}));
        EXPECT_FALSE(routed.view->visible());
        EXPECT_FALSE(routed.reset_effect());
        routed.set_view({1000, 800}, {1000, 800});
        EXPECT_FALSE(routed.route({}));
        EXPECT_FALSE(routed.route({key(KeyboardKey::S, Events::ButtonPhase::Release).data}));
        EXPECT_TRUE(routed.route({}));
        EXPECT_FALSE(routed.reset_effect());
        routed.click(position);
        EXPECT_TRUE(routed.reset_effect());
    }
}

TEST(DemoConsoleInput, RecentResultsAndNoticesReachNativeViewAndLatest) {
    RoutedConsole routed;
    const std::string query = "get practice.heal_price";
    const auto padded = std::string(DemoConsole::Model::command_bytes - query.size(), ' ') + query;
    for (std::size_t index = 0; index < 16; ++index) {
        ASSERT_TRUE(routed.model.submit(padded));
        routed.model.pump();
    }
    ASSERT_TRUE(routed.model.submit("set practice.heal_price 6789"));
    routed.model.pump();
    ASSERT_EQ(routed.practice.host->price(), 6789u);
    routed.route({key(KeyboardKey::GraveAccent).data});
    auto* output = routed.console->GetElementById("console-output");
    ASSERT_NE(output, nullptr);
    EXPECT_NE(output->GetInnerRML().find("#17 &gt; set practice.heal_price 6789"), Rml::String::npos);
    EXPECT_NE(output->GetInnerRML().find("effective: 6789"), Rml::String::npos);
    routed.model.help("set");
    routed.route({});
    EXPECT_NE(output->GetInnerRML().find("4294967295"), Rml::String::npos);
    EXPECT_FALSE(routed.model.submit("missing.command \"<\xe6\xb0\xb4\xf0\x9f\x8c\xb1>\""));
    routed.route({});
    const auto rendered = output->GetInnerRML();
    EXPECT_NE(rendered.find("Refused: unknown command"), Rml::String::npos);
    EXPECT_NE(rendered.find("&lt;\xe6\xb0\xb4\xf0\x9f\x8c\xb1&gt;"), Rml::String::npos);
    routed.route({});
    ASSERT_GT(output->GetScrollHeight(), output->GetClientHeight());
    output->SetScrollTop(0);
    routed.route({});
    ASSERT_EQ(output->GetScrollTop(), 0);
    const auto latest_activity = routed.view->activity();
    routed.click(routed.point(*routed.console->GetElementById("console-latest")));
    EXPECT_EQ(routed.view->activity(), latest_activity + 1);
    routed.route({});
    routed.route({});
    EXPECT_GT(output->GetScrollTop(), 0);
    EXPECT_GE(output->GetScrollTop() + output->GetClientHeight(), output->GetScrollHeight() - 2);
    EXPECT_EQ(output->GetInnerRML(), rendered);
    EXPECT_EQ(routed.model.entries().size(), 17u);
    EXPECT_EQ(routed.model.entries().front().id, 1u);
}
