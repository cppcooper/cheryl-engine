#include "developer-console-fixture.h"
#include "developer-console.h"
#include "developer-console-routing.h"
#include "tgui-controls.h"
#include "ui-routing.h"

#include <internals/exceptions.h>
#include <text/font-service.h>
#include <ui/rmlui/session.h>
#include <ui/tgui/session.h>

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <TGUI/Backend/Window/Backend.hpp>
#include <TGUI/Widgets/Button.hpp>
#include <TGUI/Widgets/EditBox.hpp>
#include <TGUI/Widgets/Panel.hpp>
#include <TGUI/Widgets/ScrollablePanel.hpp>

#include <array>
#include <exception>
#include <thread>
#include <utility>

#ifndef CHERYL_DEMO_TGUI
#error The coexistence producer must compile the actual TGUI route.
#endif

namespace {
    using namespace CE::Input;
    using namespace DemoConsoleTests;
    constexpr auto press = Events::ButtonPhase::Press;
    constexpr auto release = Events::ButtonPhase::Release;

    std::unique_ptr<CE::Text::FontService> fonts() {
        auto result = std::make_unique<CE::Text::FontService>();
        CE::Text::FontConfiguration configuration;
        configuration.families = {{"proof", {CE::Text::FontFile{DEMO_CONSOLE_TEST_FONT}}}};
        configuration.default_font = "proof";
        configuration.asset_directories.clear();
        configuration.system_directories = std::vector<std::filesystem::path>{};
        result->configure(configuration);
        return result;
    }

    class DualConsole final {
    public:
        NativeInputSource input;
        std::unique_ptr<CE::Text::FontService> font_service = fonts();
        CE::UI::TGUI::Session tgui{input, *font_service, 2};
        CE::UI::RmlUi::Session rml{input, 3};
        Contact::Practice practice = Contact::practice();
        DemoConsole::Model model{practice};
        std::unique_ptr<DemoTguiControls> controls;
        std::unique_ptr<DemoConsole::View> view;
        Rml::ElementDocument* original = nullptr;
        Rml::ElementDocument* console = nullptr;
        std::unique_ptr<DemoConsole::Routing> routing;
        CE::Platform::Dimensions<int> logical{1000, 800};
        CE::Platform::FramebufferSize framebuffer{1000, 800};

        explicit DualConsole(bool with_console = true) {
            if (!rml.load_font(DEMO_CONSOLE_TEST_FONT, "proof"))
                throw std::runtime_error("Cannot load the real coexistence font fixture");
            set_view(logical, framebuffer);
            rml.context().GetRootElement()->SetProperty("font-family", "proof");
            original = rml.context().LoadDocumentFromMemory(R"rml(
                <rml><head><style>
                    body { position: absolute; left: 0px; top: 0px; width: 1000px; height: 800px; margin: 0px; }
                    #panel { position: absolute; left: 0px; top: 0px; width: 300px; height: 250px; }
                    #reset { position: absolute; left: 10px; top: 10px; width: 180px; height: 32px; }
                    #field { position: absolute; left: 10px; top: 60px; width: 220px; height: 34px; }
                    #change-image { position: absolute; left: 10px; top: 110px; width: 180px; height: 32px; }
                </style></head><body><div id="panel"><button id="reset">Reset</button>
                <input id="field" type="text" /><button id="change-image">Change image</button><div id="image"></div>
                </div></body></rml>
            )rml");
            if (!original)
                throw std::runtime_error("Cannot load original native coexistence controls");
            original->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
            controls = std::make_unique<DemoTguiControls>(tgui);
            if (with_console) {
                view = std::make_unique<DemoConsole::View>(rml, model);
                console = rml.context().GetDocument(1);
            }
            routing = std::make_unique<DemoConsole::Routing>(rml, *original, view.get());
            rml.update_time(0);
            tgui.update_time(0);
        }

        void set_view(CE::Platform::Dimensions<int> next_logical, CE::Platform::FramebufferSize next_framebuffer) {
            logical = next_logical;
            framebuffer = next_framebuffer;
            rml.set_view(logical, framebuffer);
            tgui.set_view(logical, framebuffer);
        }
        bool drain(const PollSnapshot& batch) {
            const bool allowed = routing->handle_input(batch.records, DemoUiRouting::drawable(logical, framebuffer), controls.get());
            if (view)
                view->update();
            rml.update_time(0);
            tgui.update_time(0);
            return allowed;
        }
        bool route(std::initializer_list<Events::Payload> records) { return drain(*input.emit(records)); }
        bool click(Events::Pointer point) { return route({native_left(point, press), native_left(point, release)}); }
        Rml::ElementFormControlInput& rml_field() {
            return dynamic_cast<Rml::ElementFormControlInput&>(*original->GetElementById("field"));
        }
        Rml::ElementFormControlInput& console_field() {
            return dynamic_cast<Rml::ElementFormControlInput&>(*console->GetElementById("console-command"));
        }
        tgui::Panel::Ptr panel() { return std::dynamic_pointer_cast<tgui::Panel>(tgui.gui().get("demo-panel")); }
        tgui::EditBox::Ptr tgui_field() { return std::dynamic_pointer_cast<tgui::EditBox>(panel()->get("text-field")); }
        tgui::ScrollablePanel::Ptr scroll() { return std::dynamic_pointer_cast<tgui::ScrollablePanel>(panel()->get("scroll")); }
        tgui::Widget::Ptr tgui_reset() { return panel()->get("reset"); }
        tgui::Widget::Ptr tgui_image() {
            return std::dynamic_pointer_cast<tgui::Panel>(panel()->get("image-panel"))->get("change-image");
        }
        Events::Pointer point(Rml::Element& element) {
            const auto origin = element.GetAbsoluteOffset();
            return {origin.x + 4, origin.y + 4};
        }
        Events::Pointer point(const tgui::Widget::Ptr& widget) {
            const auto origin = widget->getAbsolutePosition();
            const auto size = widget->getSize();
            return {origin.x + size.x / 2, origin.y + size.y / 2};
        }
        Events::Pointer rml_reset() { return point(*original->GetElementById("reset")); }
        Events::Pointer rml_image() { return point(*original->GetElementById("change-image")); }
        Rml::String image_source() { return original->GetElementById("image")->GetAttribute<Rml::String>("src", ""); }
        void expect_no_resets() {
            EXPECT_FALSE(routing->consume_reset());
            EXPECT_FALSE(controls->consume_reset());
        }
        void prove_controls() {
            route({native_key(KeyboardKey::F4)});
            ASSERT_TRUE(rml.owns_keyboard_focus());
            ASSERT_EQ(rml.context().GetFocusElement(), &rml_field());
            route({Events::Text{U'r'}, Events::Text{U'x'}, native_key(KeyboardKey::Backspace)});
            ASSERT_EQ(rml_field().GetValue(), "r");
            click(rml_reset());
            ASSERT_TRUE(routing->consume_reset());
            ASSERT_FALSE(routing->consume_reset());
            click(rml_image());
            ASSERT_EQ(image_source(), "../textures/ui/quadrants-alt.png");
            route({native_key(KeyboardKey::F2)});
            ASSERT_TRUE(tgui.owns_keyboard_focus());
            ASSERT_FALSE(rml.owns_keyboard_focus());
            ASSERT_TRUE(tgui_field()->isFocused());
            route({Events::Text{U'a'}, Events::Text{U'b'}, native_key(KeyboardKey::Backspace)});
            ASSERT_EQ(tgui_field()->getText(), "a");
            const auto reset = point(tgui_reset());
            ASSERT_EQ(tgui.gui().getWidgetAtPos(tgui.gui().mapPixelToCoords({static_cast<int>(reset.x), static_cast<int>(reset.y)}), true),
                      tgui_reset());
            click(reset);
            ASSERT_TRUE(controls->consume_reset());
            ASSERT_FALSE(controls->consume_reset());
            click(point(tgui_image()));
            ASSERT_TRUE(controls->alternate_image());
            route({native_key(KeyboardKey::Escape)});
            ASSERT_TRUE(route({}));
        }
    };
}

TEST(DemoConsoleCoexistence, ActualOriginalControlsAndNullConsoleRemainUsable) {
    DualConsole routed{false};
    ASSERT_NO_FATAL_FAILURE(routed.prove_controls());
    EXPECT_EQ(routed.view, nullptr);
    const auto reset = routed.point(routed.tgui_reset());
    routed.route({native_key(KeyboardKey::GraveAccent), native_left(reset, press), native_left(reset, release)});
    EXPECT_TRUE(routed.controls->consume_reset());
    EXPECT_FALSE(routed.rml.owns_keyboard_focus());
    EXPECT_TRUE(routed.tgui.owns_keyboard_focus());
}

TEST(DemoConsoleCoexistence, DefaultToggleCancelsEarlierAndLaterNativeEffectsInBothToolkits) {
    DualConsole routed;
    ASSERT_NO_FATAL_FAILURE(routed.prove_controls());
    const auto rml_reset = routed.rml_reset();
    const auto rml_image = routed.rml_image();
    const auto tgui_reset = routed.point(routed.tgui_reset());
    const auto tgui_image = routed.point(routed.tgui_image());
    const auto source = routed.image_source();
    EXPECT_FALSE(routed.route({native_left(rml_reset, press), native_left(rml_reset, release),
        native_left(tgui_reset, press), native_left(tgui_reset, release), native_left(rml_image, press), native_left(rml_image, release),
        native_left(tgui_image, press), native_left(tgui_image, release), native_key(KeyboardKey::GraveAccent)}));
    ASSERT_TRUE(routed.view->visible());
    routed.expect_no_resets();
    EXPECT_EQ(routed.image_source(), source);
    EXPECT_TRUE(routed.controls->alternate_image());
    EXPECT_FALSE(routed.route({native_key(KeyboardKey::GraveAccent), native_left(tgui_reset, press), native_left(tgui_reset, release),
        native_left(rml_reset, press), native_left(rml_reset, release), native_left(tgui_image, press), native_left(tgui_image, release),
        native_left(rml_image, press), native_left(rml_image, release)}));
    EXPECT_FALSE(routed.view->visible());
    routed.expect_no_resets();
    EXPECT_EQ(routed.image_source(), source);
    EXPECT_TRUE(routed.controls->alternate_image());
    routed.click(rml_reset);
    EXPECT_TRUE(routed.routing->consume_reset());
    routed.click(tgui_reset);
    EXPECT_TRUE(routed.controls->consume_reset());
}

TEST(DemoConsoleCoexistence, HotkeyOrderDrainsOldEpochWithoutFeedingNewFields) {
    DualConsole routed;
    ASSERT_NO_FATAL_FAILURE(routed.prove_controls());
    routed.route({native_key(KeyboardKey::F2)});
    const auto old = routed.input.routing().current();
    const auto batch = routed.input.emit({Events::Text{U'p'}, native_key(KeyboardKey::F4), Events::Text{U'x'},
        native_key(KeyboardKey::GraveAccent), Events::Text{U'y'}, native_key(KeyboardKey::F2), Events::Text{U'z'}});
    ASSERT_EQ(batch->records.size(), 7u);
    for (const auto& record : batch->records) {
        ASSERT_EQ(record.target, 2u);
        ASSERT_EQ(record.focus_epoch, old->epoch);
    }
    EXPECT_FALSE(routed.drain(*batch));
    EXPECT_EQ(routed.tgui_field()->getText(), "ap");
    EXPECT_EQ(routed.rml_field().GetValue(), "r");
    EXPECT_TRUE(routed.console_field().GetValue().empty());
    EXPECT_TRUE(routed.tgui.owns_keyboard_focus());
    EXPECT_GT(routed.input.routing().current()->epoch, old->epoch);
    EXPECT_EQ(batch->records.back().target, 2u);
    EXPECT_EQ(batch->records.back().focus_epoch, old->epoch);
    routed.route({Events::Text{U'n'}});
    EXPECT_EQ(routed.tgui_field()->getText(), "apn");
    routed.route({native_key(KeyboardKey::F4)});
    routed.route({Events::Text{U'b'}, native_key(KeyboardKey::F2), Events::Text{U'x'}, native_key(KeyboardKey::F4),
                  Events::Text{U'y'}, native_key(KeyboardKey::GraveAccent), Events::Text{U'z'}});
    EXPECT_EQ(routed.rml_field().GetValue(), "rb");
    EXPECT_EQ(routed.tgui_field()->getText(), "apn");
    ASSERT_TRUE(routed.view->visible());
    EXPECT_TRUE(routed.console_field().GetValue().empty());
    routed.console_field().SetValue({});
    routed.route({Events::Text{0x6c34}, Events::Text{0x1f331}});
    EXPECT_EQ(routed.console_field().GetValue(), "\xe6\xb0\xb4\xf0\x9f\x8c\xb1");
}

TEST(DemoConsoleCoexistence, PointerRefocusCannotReviveRetiredKeyboardText) {
    DualConsole routed;
    ASSERT_NO_FATAL_FAILURE(routed.prove_controls());
    const auto rml_field = routed.point(routed.rml_field());
    const auto tgui_field = routed.point(routed.tgui_field());
    routed.route({native_key(KeyboardKey::F4)});
    const auto rml_epoch = routed.input.routing().current()->epoch;
    const auto rml_batch = routed.input.emit({native_left(tgui_field, press), native_left(tgui_field, release),
        native_left(rml_field, press), native_left(rml_field, release), Events::Text{U'x'}});
    ASSERT_EQ(rml_batch->records.back().target, 3u);
    ASSERT_EQ(rml_batch->records.back().focus_epoch, rml_epoch);
    EXPECT_FALSE(routed.drain(*rml_batch));
    EXPECT_EQ(routed.rml_field().GetValue(), "r");
    EXPECT_EQ(routed.tgui_field()->getText(), "a");
    ASSERT_TRUE(routed.rml.owns_keyboard_focus());
    EXPECT_GT(routed.input.routing().current()->epoch, rml_epoch);
    EXPECT_EQ(routed.rml.context().GetFocusElement(), &routed.rml_field());
    routed.rml_field().SetValue({});
    routed.route({Events::Text{U'n'}});
    EXPECT_EQ(routed.rml_field().GetValue(), "n");

    routed.route({native_key(KeyboardKey::F2)});
    const auto tgui_epoch = routed.input.routing().current()->epoch;
    const auto tgui_batch = routed.input.emit({native_left(rml_field, press), native_left(rml_field, release),
        native_left(tgui_field, press), native_left(tgui_field, release), Events::Text{U'y'}});
    ASSERT_EQ(tgui_batch->records.back().target, 2u);
    ASSERT_EQ(tgui_batch->records.back().focus_epoch, tgui_epoch);
    EXPECT_FALSE(routed.drain(*tgui_batch));
    EXPECT_EQ(routed.rml_field().GetValue(), "n");
    EXPECT_EQ(routed.tgui_field()->getText(), "a");
    ASSERT_TRUE(routed.tgui.owns_keyboard_focus());
    EXPECT_GT(routed.input.routing().current()->epoch, tgui_epoch);
    EXPECT_TRUE(routed.tgui_field()->isFocused());
    routed.tgui_field()->setText("");
    routed.route({Events::Text{U'n'}});
    EXPECT_EQ(routed.tgui_field()->getText(), "n");
}

TEST(DemoConsoleCoexistence, RenderPrioritySelectsOverlapAndNativeWheelOwner) {
    DualConsole routed;
    ASSERT_NO_FATAL_FAILURE(routed.prove_controls());
    const auto wheel_point = routed.point(routed.scroll());
    const auto before = routed.scroll()->getVerticalScrollbarValue();
    EXPECT_FALSE(routed.route({Events::Scroll{0, -2, wheel_point}}));
    ASSERT_GT(routed.scroll()->getVerticalScrollbarValue(), before);
    const auto origin = routed.panel()->getPosition();
    auto* panel = routed.original->GetElementById("panel");
    panel->SetProperty("left", std::to_string(static_cast<int>(origin.x)) + "px");
    panel->SetProperty("top", std::to_string(static_cast<int>(origin.y)) + "px");
    panel->SetProperty("width", "394px");
    panel->SetProperty("height", "600px");
    routed.original->GetElementById("reset")->SetProperty("left", "18px");
    routed.original->GetElementById("reset")->SetProperty("top", "148px");
    routed.rml.update_time(0);
    const auto overlap = routed.rml_reset();
    ASSERT_TRUE(routed.controls->contains(overlap));
    ASSERT_EQ(routed.tgui.gui().getWidgetAtPos(routed.tgui.gui().mapPixelToCoords(
        {static_cast<int>(overlap.x), static_cast<int>(overlap.y)}), true), routed.tgui_reset());
    routed.click(overlap);
    EXPECT_TRUE(routed.routing->consume_reset());
    EXPECT_FALSE(routed.controls->consume_reset());
    const auto scrolled = routed.scroll()->getVerticalScrollbarValue();
    EXPECT_FALSE(routed.route({Events::Scroll{0, 2, wheel_point}}));
    EXPECT_EQ(routed.scroll()->getVerticalScrollbarValue(), scrolled);
    routed.route({native_key(KeyboardKey::F6)});
    EXPECT_FALSE(routed.routing->original_visible());
    routed.route({Events::Scroll{0, 2, wheel_point}});
    EXPECT_LT(routed.scroll()->getVerticalScrollbarValue(), scrolled);
    routed.click(routed.point(routed.tgui_reset()));
    EXPECT_TRUE(routed.controls->consume_reset());
    routed.route({native_key(KeyboardKey::F3)});
    EXPECT_FALSE(routed.controls->visible());
    routed.route({native_key(KeyboardKey::F3), native_key(KeyboardKey::F6)});
    EXPECT_TRUE(routed.controls->visible());
    EXPECT_TRUE(routed.routing->original_visible());
}

TEST(DemoConsoleCoexistence, HandoffsCancelBothEffectDirectionsWithoutDisablingFreshControls) {
    DualConsole routed;
    ASSERT_NO_FATAL_FAILURE(routed.prove_controls());
    const auto rml_reset = routed.rml_reset();
    const auto tgui_reset = routed.point(routed.tgui_reset());
    const auto rml_image = routed.rml_image();
    const auto tgui_image = routed.point(routed.tgui_image());
    for (const auto hotkey : {KeyboardKey::F2, KeyboardKey::F4, KeyboardKey::GraveAccent}) {
        SCOPED_TRACE(static_cast<int>(hotkey));
        routed.route({native_key(KeyboardKey::Escape)});
        const auto image = routed.image_source();
        const auto alternate = routed.controls->alternate_image();
        EXPECT_FALSE(routed.route({native_left(tgui_reset, press), native_left(tgui_reset, release),
            native_left(rml_reset, press), native_left(rml_reset, release), native_left(tgui_image, press), native_left(tgui_image, release),
            native_left(rml_image, press), native_left(rml_image, release), native_key(hotkey),
            native_left(tgui_reset, press), native_left(tgui_reset, release), native_left(rml_reset, press), native_left(rml_reset, release),
            native_left(tgui_image, press), native_left(tgui_image, release), native_left(rml_image, press), native_left(rml_image, release)}));
        routed.expect_no_resets();
        EXPECT_EQ(routed.image_source(), image);
        EXPECT_EQ(routed.controls->alternate_image(), alternate);
        routed.route({native_key(KeyboardKey::Escape)});
        routed.click(tgui_reset);
        EXPECT_TRUE(routed.controls->consume_reset());
        routed.click(rml_reset);
        EXPECT_TRUE(routed.routing->consume_reset());
    }
}

TEST(DemoConsoleCoexistence, HeldKeysModifiersAndOldLeftGesturesObserveRealReleases) {
    DualConsole routed;
    ASSERT_NO_FATAL_FAILURE(routed.prove_controls());
    const auto tgui_reset = routed.point(routed.tgui_reset());
    auto shifted_press = native_left(tgui_reset, press);
    shifted_press.modifiers = Modifiers::Shift;
    routed.route({native_key(KeyboardKey::W), native_key(KeyboardKey::LeftShift, press, Modifiers::Shift), shifted_press});
    EXPECT_TRUE(routed.tgui.gui().isKeyboardModifierPressed(tgui::Event::KeyModifier::Shift));
    routed.expect_no_resets();
    routed.route({native_key(KeyboardKey::GraveAccent), native_key(KeyboardKey::Escape)});
    EXPECT_FALSE(routed.route({native_left(tgui_reset, release), native_key(KeyboardKey::LeftShift, release)}));
    routed.expect_no_resets();
    EXPECT_FALSE(routed.tgui.gui().isKeyboardModifierPressed(tgui::Event::KeyModifier::Shift));
    EXPECT_FALSE(routed.route({}));
    EXPECT_FALSE(routed.route({native_key(KeyboardKey::W, release)}));
    EXPECT_TRUE(routed.route({}));
    routed.click(tgui_reset);
    EXPECT_TRUE(routed.controls->consume_reset());
    const auto rml_reset = routed.rml_reset();
    routed.route({native_left(rml_reset, press)});
    routed.route({native_key(KeyboardKey::F2), native_key(KeyboardKey::Escape)});
    EXPECT_FALSE(routed.route({native_left(rml_reset, release)}));
    routed.expect_no_resets();
    routed.route({native_left(rml_reset, release)});
    routed.expect_no_resets();
    routed.click(rml_reset);
    EXPECT_TRUE(routed.routing->consume_reset());
}

TEST(DemoConsoleCoexistence, HeldConsoleButtonCannotSubmitAfterCloseAndReopen) {
    DualConsole routed;
    ASSERT_NO_FATAL_FAILURE(routed.prove_controls());
    routed.route({native_key(KeyboardKey::GraveAccent)});
    routed.console_field().SetValue("set practice.heal_price 99");
    const auto submit = routed.point(*routed.console->GetElementById("console-submit"));
    const auto price = routed.practice.host->price();
    routed.route({native_left(submit, press)});
    ASSERT_TRUE(routed.model.entries().empty());
    routed.route({native_key(KeyboardKey::GraveAccent), native_key(KeyboardKey::GraveAccent)});
    ASSERT_TRUE(routed.view->visible());
    EXPECT_FALSE(routed.route({native_left(submit, release)}));
    routed.model.pump();
    EXPECT_TRUE(routed.model.entries().empty());
    EXPECT_EQ(routed.practice.host->price(), price);
    routed.click(submit);
    routed.model.pump();
    ASSERT_EQ(routed.model.entries().size(), 1u);
    EXPECT_EQ(routed.practice.host->price(), 99u);
}

TEST(DemoConsoleCoexistence, NativeCloseOutsideAndEscapeCancelUnderlyingEffects) {
    DualConsole routed;
    ASSERT_NO_FATAL_FAILURE(routed.prove_controls());
    const auto reset = routed.point(routed.tgui_reset());
    const auto original_reset = routed.rml_reset();
    for (int mode = 0; mode < 3; ++mode) {
        SCOPED_TRACE(mode);
        routed.route({native_key(KeyboardKey::Escape)});
        routed.route({native_key(KeyboardKey::GraveAccent)});
        ASSERT_TRUE(routed.view->visible());
        if (mode == 0) {
            auto* close = routed.console->GetElementById("console-close");
            const auto position = routed.point(*close);
            auto* hit = routed.rml.context().GetElementAtPoint({static_cast<float>(position.x), static_cast<float>(position.y)});
            while (hit && hit != close)
                hit = hit->GetParentNode();
            ASSERT_EQ(hit, close);
            ASSERT_TRUE(routed.view->inside(position));
            const auto activity = routed.view->activity();
            routed.route({native_left(position, press), native_left(position, release), native_left(reset, press), native_left(reset, release)});
            EXPECT_EQ(routed.view->activity(), activity + 2);
        } else if (mode == 1) {
            ASSERT_FALSE(routed.view->inside(original_reset));
            routed.route({native_left(original_reset, press), native_left(original_reset, release),
                          native_left(reset, press), native_left(reset, release)});
        } else
            routed.route({native_key(KeyboardKey::Escape), native_left(reset, press), native_left(reset, release),
                          native_left(original_reset, press), native_left(original_reset, release)});
        EXPECT_FALSE(routed.view->visible());
        routed.expect_no_resets();
        EXPECT_TRUE(routed.route({}));
        routed.click(reset);
        EXPECT_TRUE(routed.controls->consume_reset());
        routed.click(original_reset);
        EXPECT_TRUE(routed.routing->consume_reset());
    }
}

TEST(DemoConsoleCoexistence, PreemptionAndIndependentZeroExtentsCancelWithoutReplay) {
    DualConsole routed;
    ASSERT_NO_FATAL_FAILURE(routed.prove_controls());
    routed.route({native_key(KeyboardKey::GraveAccent), native_key(KeyboardKey::W)});
    const auto old = routed.input.emit({Events::Text{U'x'}});
    auto successor = routed.input.routing().focus(99);
    EXPECT_FALSE(routed.drain(*old));
    EXPECT_TRUE(successor.owns_focus());
    EXPECT_FALSE(routed.view->visible());
    EXPECT_TRUE(routed.console_field().GetValue().empty());
    routed.expect_no_resets();
    successor.reset();
    routed.route({native_key(KeyboardKey::W, release)});
    EXPECT_TRUE(routed.route({}));
    const std::array extents{
        std::pair{CE::Platform::Dimensions<int>{0, 800}, CE::Platform::FramebufferSize{1000, 800}},
        std::pair{CE::Platform::Dimensions<int>{1000, 0}, CE::Platform::FramebufferSize{1000, 800}},
        std::pair{CE::Platform::Dimensions<int>{1000, 800}, CE::Platform::FramebufferSize{0, 800}},
        std::pair{CE::Platform::Dimensions<int>{1000, 800}, CE::Platform::FramebufferSize{1000, 0}}
    };
    const auto rml_reset = routed.rml_reset();
    const auto tgui_reset = routed.point(routed.tgui_reset());
    for (const auto& [logical, framebuffer] : extents) {
        routed.route({native_key(KeyboardKey::GraveAccent), native_key(KeyboardKey::S)});
        ASSERT_TRUE(routed.view->visible());
        routed.set_view(logical, framebuffer);
        ASSERT_FALSE(DemoUiRouting::drawable(logical, framebuffer));
        EXPECT_FALSE(routed.route({native_left(tgui_reset, press), native_left(tgui_reset, release),
                                  native_left(rml_reset, press), native_left(rml_reset, release)}));
        EXPECT_FALSE(routed.view->visible());
        routed.expect_no_resets();
        routed.set_view({1000, 800}, {1000, 800});
        EXPECT_FALSE(routed.route({}));
        routed.route({native_key(KeyboardKey::S, release)});
        EXPECT_TRUE(routed.route({}));
        routed.click(tgui_reset);
        EXPECT_TRUE(routed.controls->consume_reset());
        routed.click(rml_reset);
        EXPECT_TRUE(routed.routing->consume_reset());
        routed.route({native_key(KeyboardKey::Escape)});
    }
}

TEST(DemoConsoleCoexistence, NativeOwnersDetachWhileSessionsAndOwnedResultsRemainLive) {
    DualConsole routed;
    ASSERT_NO_FATAL_FAILURE(routed.prove_controls());
    routed.route({native_key(KeyboardKey::GraveAccent)});
    routed.console_field().SetValue("set practice.heal_price 27");
    routed.route({native_key(KeyboardKey::Enter)});
    routed.model.pump();
    ASSERT_EQ(routed.model.entries().size(), 1u);
    ASSERT_EQ(routed.model.entries()[0].result.stage, Core::Stage::Succeeded);
    const auto retained = routed.model.entries()[0].id;
    auto retired_panel = routed.panel();
    const auto reset = routed.point(routed.tgui_reset());
    routed.routing.reset();
    routed.view->hide();
    routed.view.reset();
    routed.console = nullptr;
    routed.controls.reset();
    EXPECT_EQ(routed.tgui.gui().get("demo-panel"), nullptr);
    EXPECT_EQ(routed.tgui.gui().get("edge-tooltip"), nullptr);
    routed.tgui.gui().add(retired_panel, "retired-controls");
    const auto old_click = routed.input.emit({native_left(reset, press), native_left(reset, release)});
    routed.tgui.handle_input(old_click->records, true);
    routed.tgui.gui().remove(retired_panel);
    retired_panel.reset();
    routed.controls = std::make_unique<DemoTguiControls>(routed.tgui);
    routed.routing = std::make_unique<DemoConsole::Routing>(routed.rml, *routed.original, nullptr);
    routed.click(routed.point(routed.tgui_reset()));
    EXPECT_TRUE(routed.controls->consume_reset());
    routed.click(routed.rml_reset());
    EXPECT_TRUE(routed.routing->consume_reset());
    ASSERT_EQ(routed.model.entries().size(), 1u);
    EXPECT_EQ(routed.model.entries()[0].id, retained);
    EXPECT_EQ(routed.practice.host->price(), 27u);
    EXPECT_EQ(routed.model.entries()[0].result.stage, Core::Stage::Succeeded);
    routed.model.close();
}

TEST(DemoConsoleCoexistence, SimulationRetirementSurvivesJoinedPlatformShutdown) {
    for (const bool with_console : {false, true}) {
        SCOPED_TRACE(with_console ? "console present" : "null console");
        std::unique_ptr<DualConsole> retired;
        std::exception_ptr failure;
        std::thread simulation([&] {
            try {
                auto routed = std::make_unique<DualConsole>(with_console);
                const auto exercise = [&] {
                    ASSERT_NO_FATAL_FAILURE(routed->prove_controls());
                    ASSERT_TRUE(routed->model.submit("set practice.heal_price 27"));
                    routed->model.pump();
                    ASSERT_EQ(routed->model.entries().size(), 1u);
                    ASSERT_EQ(routed->model.entries()[0].result.stage, Core::Stage::Succeeded);
                    auto panel = routed->panel();
                    auto reset = std::dynamic_pointer_cast<tgui::Button>(routed->tgui_reset());
                    auto image = std::dynamic_pointer_cast<tgui::Button>(routed->tgui_image());
                    ASSERT_NE(panel, nullptr);
                    ASSERT_NE(reset, nullptr);
                    ASSERT_NE(image, nullptr);
                    const std::weak_ptr<tgui::Widget> panel_lifetime = panel;
                    const std::weak_ptr<tgui::Widget> field_lifetime = routed->tgui_field();
                    const std::weak_ptr<tgui::Widget> picture_lifetime =
                        std::dynamic_pointer_cast<tgui::Panel>(panel->get("image-panel"))->get("image");
                    const std::weak_ptr<tgui::Widget> edge_lifetime = routed->tgui.gui().get("edge-tooltip");
                    ASSERT_FALSE(edge_lifetime.expired());
                    const auto presses = std::make_shared<std::array<unsigned int, 2>>(std::array{0u, 0u});
                    reset->onPress([presses] { ++(*presses)[0]; });
                    image->onPress([presses] { ++(*presses)[1]; });
                    routed->controls->begin_input(true);
                    routed->controls->select_effects(true);
                    const auto pending = routed->input.emit({
                        native_left(routed->point(reset), press), native_left(routed->point(reset), release),
                        native_left(routed->point(image), press), native_left(routed->point(image), release)
                    });
                    routed->tgui.handle_input(pending->records, true);
                    ASSERT_EQ(*presses, (std::array{1u, 1u}));
                    ASSERT_TRUE(routed->controls->alternate_image());
                    routed->controls->retire();
                    EXPECT_EQ(routed->tgui.gui().get("demo-panel"), nullptr);
                    EXPECT_EQ(routed->tgui.gui().get("edge-tooltip"), nullptr);
                    EXPECT_TRUE(edge_lifetime.expired());
                    EXPECT_FALSE(routed->controls->visible());
                    EXPECT_FALSE(routed->controls->consume_reset());
                    routed->controls->finish_input();
                    EXPECT_TRUE(routed->controls->alternate_image());
                    routed->tgui.gui().add(panel, "retired-controls");
                    routed->controls->begin_input(true);
                    routed->controls->select_effects(true);
                    const auto stale = routed->input.emit({
                        native_left(routed->point(reset), press), native_left(routed->point(reset), release),
                        native_left(routed->point(image), press), native_left(routed->point(image), release)
                    });
                    routed->tgui.handle_input(stale->records, true);
                    EXPECT_EQ(*presses, (std::array{2u, 2u}));
                    routed->controls->finish_input();
                    EXPECT_FALSE(routed->controls->consume_reset());
                    EXPECT_TRUE(routed->controls->alternate_image());
                    routed->controls->retire();
                    routed->tgui.gui().remove(panel);
                    panel.reset();
                    reset.reset();
                    image.reset();
                    EXPECT_TRUE(panel_lifetime.expired());
                    EXPECT_TRUE(field_lifetime.expired());
                    EXPECT_TRUE(picture_lifetime.expired());
                };
                exercise();
                routed->controls->retire();
                routed->routing->retire();
                routed->routing->retire();
                routed->routing.reset();
                if (routed->view) {
                    routed->view->hide();
                    routed->view.reset();
                    routed->console = nullptr;
                }
                retired = std::move(routed);
            } catch (...) {
                failure = std::current_exception();
            }
        });
        const auto owner = simulation.get_id();
        simulation.join();
        if (failure)
            std::rethrow_exception(failure);
        ASSERT_NE(owner, std::this_thread::get_id());
        ASSERT_NE(retired, nullptr);
        EXPECT_THROW(static_cast<void>(retired->tgui.gui()), CE::Exceptions::failed_operation);
        EXPECT_NO_THROW(retired->controls->retire());
        EXPECT_NO_THROW(retired->controls->retire());
        EXPECT_NO_THROW(retired->controls->set_visible(true));
        EXPECT_FALSE(retired->controls->visible());
        EXPECT_FALSE(retired->controls->contains({600, 200}));
        EXPECT_FALSE(retired->controls->field_contains({600, 200}));
        EXPECT_NO_THROW(retired->controls->focus_field());
        EXPECT_NO_THROW(retired->controls->update_status({}));
        EXPECT_NO_THROW(retired->controls->begin_input(true));
        EXPECT_NO_THROW(retired->controls->select_effects(true));
        EXPECT_NO_THROW(retired->controls->finish_input());
        EXPECT_FALSE(retired->controls->consume_reset());
        EXPECT_NO_THROW(retired->controls->cancel_effects());
        EXPECT_NO_THROW(retired->controls->handle_input({}, true));
        EXPECT_FALSE(retired->controls->visible());
        EXPECT_NO_THROW(retired->controls.reset());
        ASSERT_EQ(retired->model.entries().size(), 1u);
        const auto retained_result = retired->model.entries()[0];
        EXPECT_EQ(retired->practice.host->price(), 27u);
        retired->model.close();
        EXPECT_NO_THROW(retired->tgui.close_after_quiescence());
        EXPECT_NO_THROW(retired->rml.close_after_quiescence());
        EXPECT_FALSE(tgui::isBackendSet());
        retired.reset();
        EXPECT_EQ(retained_result.id, 1u);
        EXPECT_EQ(retained_result.line, "set practice.heal_price 27");
        EXPECT_EQ(retained_result.result.stage, Core::Stage::Succeeded);
        EXPECT_EQ(number(retained_result.result, 1), 27u);
    }
}

TEST(DemoConsoleCoexistence, LiveForeignRetirementLeavesOwnerControlsUsable) {
    DualConsole routed{false};
    ASSERT_NO_FATAL_FAILURE(routed.prove_controls());
    const auto panel = routed.panel();
    const auto edge = routed.tgui.gui().get("edge-tooltip");
    std::thread foreign([&] { EXPECT_THROW(routed.controls->retire(), CE::Exceptions::failed_operation); });
    foreign.join();
    EXPECT_EQ(routed.tgui.gui().get("demo-panel"), panel);
    EXPECT_EQ(routed.tgui.gui().get("edge-tooltip"), edge);
    EXPECT_TRUE(routed.controls->visible());
    routed.click(routed.point(routed.tgui_reset()));
    EXPECT_TRUE(routed.controls->consume_reset());
    EXPECT_FALSE(routed.controls->consume_reset());
    routed.click(routed.point(routed.tgui_image()));
    EXPECT_FALSE(routed.controls->alternate_image());
    routed.controls->retire();
    routed.controls->retire();
    EXPECT_EQ(routed.tgui.gui().get("demo-panel"), nullptr);
    EXPECT_EQ(routed.tgui.gui().get("edge-tooltip"), nullptr);
}

TEST(DemoConsoleCoexistence, SessionOnlyInitializationSurvivesJoinedTeardown) {
    NativeInputSource input;
    const auto font_service = fonts();
    std::unique_ptr<CE::UI::TGUI::Session> session;
    std::exception_ptr failure;
    std::thread simulation([&] {
        try {
            session = std::make_unique<CE::UI::TGUI::Session>(input, *font_service, 2);
            EXPECT_EQ(session->gui().get("demo-panel"), nullptr);
            EXPECT_EQ(session->gui().get("edge-tooltip"), nullptr);
        } catch (...) {
            failure = std::current_exception();
        }
    });
    simulation.join();
    if (failure)
        std::rethrow_exception(failure);
    ASSERT_NE(session, nullptr);
    EXPECT_THROW(static_cast<void>(session->gui()), CE::Exceptions::failed_operation);
    EXPECT_NO_THROW(session->close_after_quiescence());
    EXPECT_NO_THROW(session.reset());
    EXPECT_FALSE(tgui::isBackendSet());
}

TEST(DemoConsoleCoexistence, FailedControlsConstructionPreservesClosedSessionFailure) {
    NativeInputSource input;
    const auto font_service = fonts();
    CE::UI::TGUI::Session session{input, *font_service, 2};
    session.close_after_quiescence();
    ASSERT_FALSE(tgui::isBackendSet());
    EXPECT_THROW(static_cast<void>(DemoTguiControls{session}), CE::Exceptions::failed_operation);
    EXPECT_FALSE(tgui::isBackendSet());
    EXPECT_THROW(static_cast<void>(session.gui()), CE::Exceptions::failed_operation);
}
