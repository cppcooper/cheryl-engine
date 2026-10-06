#include <ui/rmlui/session.h>
#include <internals/exceptions.h>
#include "../../support/document.h"

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/EventListener.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <exception>
#include <fstream>
#include <iterator>
#include <limits>
#include <thread>

namespace {
    using namespace CE::Input;
    using namespace CE::UI::RmlUi;

    class Input final : public iInputSystem {
        InputBindings bindings_;

    public:
        bool text_supported = true;
        bool focus_supported = true;

        void initialize(CE::iWindow&) override {}
        void poll() override {}
        void deinitialize() override {}
        InputBindings& bindings() override { return bindings_; }
        DeviceId keyboard_id() const override { return 1; }
        DeviceId mouse_id() const override { return 2; }
        DeviceId gamepad_id() const override { return 3; }
        bool supports(const InputMode mode) const override { return mode != InputMode::Text || text_supported; }
        bool supports_focus() const override { return focus_supported; }
        std::shared_ptr<const PollSnapshot> emit(InputRecordData data, const DeviceKind kind = DeviceKind::Keyboard) {
            begin_input_poll();
            capture_buffer().record(kind == DeviceKind::Mouse ? mouse_id() : keyboard_id(), kind, std::move(data));
            return publish_input();
        }
    };

    ButtonEvent key(const KeyboardKey key, const Modifiers flags = Modifiers::None, const ButtonPhase phase = ButtonPhase::Press) {
        return {800, phase, flags, -1, -1, key};
    }
}

TEST(ui_rmlui_session, lifecycle) {
    Input input;
    EXPECT_EQ(Rml::GetSystemInterface(), nullptr);
    {
        Session session(input, 7);
        EXPECT_NE(Rml::GetSystemInterface(), nullptr);
        EXPECT_THROW(Session(input, 8), CE::Exceptions::failed_operation);
        session.request_keyboard_focus();
        const auto capabilities = session.capabilities();
        EXPECT_TRUE(capabilities.keyboard_focus);
        EXPECT_TRUE(capabilities.committed_text);
        EXPECT_TRUE(capabilities.rectangular_clipping);
        EXPECT_FALSE(capabilities.clipboard);
        EXPECT_FALSE(capabilities.cursor);
        EXPECT_FALSE(capabilities.ime);
        EXPECT_FALSE(capabilities.effects);
        const auto text = input.emit(TextEvent{U'a'});
        ASSERT_EQ(text->records.size(), 1u);
        EXPECT_EQ(text->records[0].target, 7u);
        session.release_keyboard_focus();
        EXPECT_TRUE(input.emit(TextEvent{U'a'})->records.empty());
    }
    EXPECT_EQ(Rml::GetSystemInterface(), nullptr);
    EXPECT_TRUE(input.emit(key(KeyboardKey::A))->records.empty());
    Session next(input, 9);
    EXPECT_NE(Rml::GetSystemInterface(), nullptr);
}

TEST(ui_rmlui_session, routing) {
    Input input;
    Session session(input, 7);
    auto& document = RmlUiTests::document(session);
    auto& field = RmlUiTests::field(document);
    session.request_keyboard_focus();
    ASSERT_TRUE(field.Focus());
    const auto first = input.emit(TextEvent{U'a'});
    session.handle_input(first->records, false);
    EXPECT_EQ(field.GetValue(), "a");
    auto foreign = first->records;
    foreign[0].target = 9;
    session.handle_input(foreign, false);
    EXPECT_EQ(field.GetValue(), "a");
    session.request_keyboard_focus();
    session.handle_input(first->records, false);
    EXPECT_EQ(field.GetValue(), "a");
    const auto pending = input.emit(TextEvent{U'b'});
    auto newer = input.routing().focus(9);
    session.handle_input(pending->records, false);
    EXPECT_EQ(field.GetValue(), "ab");
    EXPECT_FALSE(session.owns_keyboard_focus());
    EXPECT_TRUE(newer.owns_focus());
    EXPECT_EQ(first->records[0].target, 7u);
}

TEST(ui_rmlui_session, editing) {
    Input input;
    Session session(input, 7);
    auto& document = RmlUiTests::document(session);
    auto& field = RmlUiTests::field(document);
    session.request_keyboard_focus();
    ASSERT_TRUE(field.Focus());
    for (const auto character : {U'a', U'é', U'b'})
        session.handle_input(input.emit(TextEvent{character})->records, false);
    EXPECT_EQ(
        field.GetValue(), "a\xc3\xa9"
                          "b"
    );
    session.handle_input(input.emit(key(KeyboardKey::Backspace, Modifiers::None, ButtonPhase::Repeat))->records, false);
    EXPECT_EQ(field.GetValue(), "a\xc3\xa9");
    session.handle_input(input.emit(key(KeyboardKey::Left, Modifiers::Shift))->records, false);
    session.handle_input(input.emit(TextEvent{U'x'})->records, false);
    EXPECT_EQ(field.GetValue(), "ax");
    field.SetValue("keep");
    field.Select();
    for (const auto shortcut : {KeyboardKey::C, KeyboardKey::X, KeyboardKey::V}) {
        session.handle_input(input.emit(key(shortcut, Modifiers::Control))->records, false);
        EXPECT_EQ(field.GetValue(), "keep");
    }
    EXPECT_THROW(Rml::GetSystemInterface()->SetClipboardText("private"), CE::Exceptions::failed_operation);
    EXPECT_THROW(session.handle_input(input.emit(TextEvent{0xD800})->records, false), CE::Exceptions::invalid_args);
}

TEST(ui_rmlui_session, click_position) {
    Input input;
    Session session(input, 7);
    auto& document = RmlUiTests::document(session);
    auto& field = RmlUiTests::field(document);
    field.Blur();
    const ButtonEvent click{900, ButtonPhase::Press,   Modifiers::None,   -1,
                            -1,  KeyboardKey::Unknown, MouseButton::Left, PointerEvent{20, 45}};
    session.handle_input(input.emit(click, DeviceKind::Mouse)->records, false);
    EXPECT_NE(session.context().GetFocusElement(), &field);
    // A first click carries its own position; no earlier movement is necessary.
    session.handle_input(input.emit(click, DeviceKind::Mouse)->records, true);
    EXPECT_EQ(session.context().GetFocusElement(), &field);
    auto release = click;
    release.phase = ButtonPhase::Release;
    session.handle_input(input.emit(release, DeviceKind::Mouse)->records, true);
    session.handle_input(input.emit(PointerEvent{300, 200}, DeviceKind::Mouse)->records, true);
    EXPECT_EQ(session.context().GetFocusElement(), &field);
    auto missing = click;
    missing.position.reset();
    EXPECT_THROW(session.handle_input(input.emit(missing, DeviceKind::Mouse)->records, true), CE::Exceptions::invalid_args);
}

TEST(ui_rmlui_session, view_time) {
    Input input;
    Session session(input, 7);
    RmlUiTests::document(session);
    const auto first = session.record();
    EXPECT_FALSE(first.draws().empty());
    EXPECT_EQ(first.width(), 320);
    EXPECT_EQ(session.context().GetDimensions(), (Rml::Vector2i{320, 240}));
    session.update_time(0.25);
    session.update_time(0.5);
    EXPECT_DOUBLE_EQ(Rml::GetSystemInterface()->GetElapsedTime(), 0.75);
    session.set_view({160, 120}, {200, 150});
    EXPECT_EQ(session.record().width(), 160);
    EXPECT_EQ(first.width(), 320);
    session.set_view({160, 120}, {0, 0});
    EXPECT_TRUE(session.record().draws().empty());
    session.set_view({0, 0}, {0, 0});
    EXPECT_TRUE(session.record().draws().empty());
    EXPECT_THROW(session.update_time(-1), CE::Exceptions::invalid_args);
    EXPECT_THROW(session.update_time(std::numeric_limits<double>::infinity()), CE::Exceptions::invalid_args);
    EXPECT_THROW(session.set_view({-1, 2}, {2, 2}), CE::Exceptions::invalid_args);
}

TEST(ui_rmlui_session, font_lifetime) {
    Input input;
    RecordedScene retained;
    std::vector<unsigned char> saved;
    {
        Session session(input, 7);
        std::ifstream file(CHERYL_RMLUI_TEST_FONT, std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        ASSERT_FALSE(bytes.empty());
        ASSERT_TRUE(session.load_font(std::span<const unsigned char>{bytes}, "owned"));
        bytes.clear();
        bytes.shrink_to_fit();
        session.set_view({320, 240}, {640, 480});
        auto* document = session.context().LoadDocumentFromMemory(
            "<rml><head><style>body { font-family: owned; font-size: 24px; }</style></head><body>ABC</body></rml>"
        );
        ASSERT_TRUE(document);
        document->Show();
        retained = session.record();
        ASSERT_FALSE(retained.draws().empty());
        ASSERT_TRUE(retained.draws()[0].texture);
        saved = retained.draws()[0].texture->rgba;
        EXPECT_TRUE(std::any_of(saved.begin(), saved.end(), [](const auto channel) { return channel != 0; }));
        document->SetInnerRML("ABC éàö0123456789");
        const auto replacement = session.record();
        EXPECT_FALSE(replacement.draws().empty());
        EXPECT_EQ(retained.draws()[0].texture->rgba, saved);
    }
    EXPECT_EQ(Rml::GetSystemInterface(), nullptr);
    ASSERT_FALSE(retained.draws().empty());
    EXPECT_EQ(retained.draws()[0].texture->rgba, saved);
}

TEST(ui_rmlui_session, unavailable) {
    Input input;
    input.text_supported = false;
    input.focus_supported = false;
    Session session(input, 7);
    EXPECT_FALSE(session.capabilities().committed_text);
    EXPECT_FALSE(session.capabilities().keyboard_focus);
    EXPECT_THROW(session.request_keyboard_focus(), CE::Exceptions::failed_operation);
}

TEST(ui_rmlui_session, wheel_position) {
    Input input;
    Session session(input, 7);
    auto& document = RmlUiTests::document(session);
    struct Listener final : Rml::EventListener {
        int calls = 0;
        float x = 0;
        float y = 0;
        int mouse_x = 0;
        int mouse_y = 0;

        void ProcessEvent(Rml::Event& event) override {
            ++calls;
            x = event.GetParameter<float>("wheel_delta_x", 0);
            y = event.GetParameter<float>("wheel_delta_y", 0);
            mouse_x = event.GetParameter<int>("mouse_x", 0);
            mouse_y = event.GetParameter<int>("mouse_y", 0);
            event.StopPropagation();
        }
    } listener;
    document.AddEventListener("mousescroll", &listener, true);
    const auto poll = input.emit(ScrollEvent{0.5, -0.25, PointerEvent{20.75, 45.25}}, DeviceKind::Mouse);
    session.handle_input(poll->records, false);
    EXPECT_EQ(listener.calls, 0);
    session.handle_input(poll->records, true);
    EXPECT_EQ(listener.calls, 1);
    EXPECT_FLOAT_EQ(listener.x, -0.5f);
    EXPECT_FLOAT_EQ(listener.y, 0.25f);
    EXPECT_EQ(listener.mouse_x, 20);
    EXPECT_EQ(listener.mouse_y, 45);
    document.RemoveEventListener("mousescroll", &listener, true);
}

TEST(ui_rmlui_session, font_bound) {
    Input input;
    EXPECT_THROW(Session(input, 7, {.maximum_texture_size = 512}), CE::Exceptions::invalid_args);
    EXPECT_EQ(Rml::GetSystemInterface(), nullptr);
}

TEST(ui_rmlui_session, owner) {
    Input input;
    Session session(input, 7);
    std::exception_ptr failure;
    std::thread foreign([&] {
        try {
            (void)session.context();
        } catch (...) {
            failure = std::current_exception();
        }
    });
    foreign.join();
    ASSERT_TRUE(failure);
    EXPECT_THROW(std::rethrow_exception(failure), CE::Exceptions::failed_operation);
    session.close_after_quiescence();
    EXPECT_EQ(Rml::GetSystemInterface(), nullptr);
    EXPECT_THROW(static_cast<void>(session.context()), CE::Exceptions::failed_operation);
}

TEST(ui_rmlui_session, host_interface) {
    Input input;
    Rml::SystemInterface host;
    Rml::SetSystemInterface(&host);
    EXPECT_THROW(Session(input, 7), CE::Exceptions::failed_operation);
    EXPECT_EQ(Rml::GetSystemInterface(), &host);
    Rml::SetSystemInterface(nullptr);
}
