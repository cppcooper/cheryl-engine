#include <gtest/gtest.h>

#include <ui/tgui/session.h>
#include <internals/exceptions.h>

#include <TGUI/Backend/Window/Backend.hpp>
#include <TGUI/Widgets/EditBox.hpp>
#include <TGUI/Widgets/Label.hpp>

#include <limits>
#include <thread>

namespace {
    using namespace CE::Input;
    using namespace CE::UI::TGUI;

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

        std::shared_ptr<const PollSnapshot> emit(const InputRecordData data, const DeviceKind kind = DeviceKind::Keyboard) {
            begin_input_poll();
            capture_buffer().record(kind == DeviceKind::Mouse ? mouse_id() : keyboard_id(), kind, data);
            return publish_input();
        }
    };

    ButtonEvent key(const KeyboardKey id, const Modifiers modifiers = Modifiers::None, const ButtonPhase phase = ButtonPhase::Press) {
        return {800, phase, modifiers, -1, -1, id};
    }
}

TEST(ui_tgui_session, lifecycle) {
    Input input;
    EXPECT_FALSE(tgui::isBackendSet());
    {
        Session session(input, 7);
        EXPECT_TRUE(tgui::isBackendSet());
        EXPECT_THROW(Session(input, 8), CE::Exceptions::failed_operation);
        EXPECT_EQ(input.emit(key(KeyboardKey::A))->records.size(), 1u);
        session.request_keyboard_focus();
        const auto capabilities = session.capabilities();
        EXPECT_TRUE(capabilities.keyboard_focus);
        EXPECT_TRUE(capabilities.committed_text);
        EXPECT_FALSE(capabilities.clipboard);
        EXPECT_FALSE(capabilities.cursor);
        EXPECT_FALSE(capabilities.ime);
        EXPECT_TRUE(session.owns_keyboard_focus());
        const auto text = input.emit(TextEvent{U'a'});
        ASSERT_EQ(text->records.size(), 1u);
        EXPECT_EQ(text->records[0].target, 7u);
        session.release_keyboard_focus();
        EXPECT_FALSE(session.owns_keyboard_focus());
        EXPECT_TRUE(input.emit(TextEvent{U'a'})->records.empty());
    }
    EXPECT_FALSE(tgui::isBackendSet());
    EXPECT_TRUE(input.emit(key(KeyboardKey::A))->records.empty());
    Session next(input, 9);
    EXPECT_TRUE(tgui::isBackendSet());
}

TEST(ui_tgui_session, routing) {
    Input input;
    Session session(input, 7);
    session.set_view({320, 240}, {640, 480});
    const auto field = tgui::EditBox::create();
    field->setSize({200, 30});
    session.gui().add(field);
    session.request_keyboard_focus();
    field->setFocused(true);
    const auto first = input.emit(TextEvent{U'a'});
    session.handle_input(first->records, false);
    EXPECT_EQ(field->getText(), "a");
    auto foreign = first->records;
    foreign[0].target = 9;
    session.handle_input(foreign, false);
    EXPECT_EQ(field->getText(), "a");
    session.request_keyboard_focus();
    session.handle_input(first->records, false); // Same target, obsolete lease.
    EXPECT_EQ(field->getText(), "a");
    const auto pending = input.emit(TextEvent{U'b'});
    auto newer_owner = input.routing().focus(9);
    session.handle_input(pending->records, false); // Poll-latched delivery drains.
    EXPECT_EQ(field->getText(), "ab");
    EXPECT_FALSE(session.owns_keyboard_focus());
    EXPECT_TRUE(newer_owner.owns_focus());
    EXPECT_EQ(first->records[0].target, 7u); // Shared records were not consumed.
}

TEST(ui_tgui_session, modifiers_clipboard) {
    Input input;
    Session session(input, 7);
    session.set_view({320, 240}, {320, 240});
    const auto field = tgui::EditBox::create();
    field->setSize({200, 30});
    field->setText("keep");
    session.gui().add(field);
    session.request_keyboard_focus();
    field->setFocused(true);
    field->selectText();
#ifdef TGUI_SYSTEM_MACOS
    constexpr auto modifier = Modifiers::Super;
#else
    constexpr auto modifier = Modifiers::Control;
#endif
    for (const auto shortcut : {KeyboardKey::C, KeyboardKey::X, KeyboardKey::V}) {
        const auto poll = input.emit(key(shortcut, modifier));
        session.handle_input(poll->records, false);
        EXPECT_EQ(field->getText(), "keep");
    }
    const auto shift = input.emit(key(KeyboardKey::LeftShift, Modifiers::Shift));
    session.handle_input(shift->records, false);
    EXPECT_TRUE(session.gui().isKeyboardModifierPressed(tgui::Event::KeyModifier::Shift));
    auto release = input.emit(key(KeyboardKey::LeftShift, Modifiers::None, ButtonPhase::Release))->records;
    release[0].target = 99; // Even an unselected release updates the snapshot.
    session.handle_input(release, false);
    EXPECT_FALSE(session.gui().isKeyboardModifierPressed(tgui::Event::KeyModifier::Shift));
    EXPECT_THROW(tgui::getBackend()->setClipboard("private"), CE::Exceptions::failed_operation);
    EXPECT_THROW(tgui::getBackend()->getClipboard(), CE::Exceptions::failed_operation);
}

TEST(ui_tgui_session, views) {
    Input input;
    Session session(input, 7);
    session.set_view({320, 240}, {640, 480});
    const auto label = tgui::Label::create("Copied view");
    session.gui().add(label);
    const auto first = session.record();
    EXPECT_EQ(session.gui().getContainer()->getSize(), (tgui::Vector2f{320, 240}));
    EXPECT_EQ(session.gui().mapPixelToCoords({40, 30}), (tgui::Vector2f{40, 30}));
    EXPECT_FLOAT_EQ(first.width(), 320);
    EXPECT_FLOAT_EQ(first.height(), 240);
    EXPECT_FALSE(first.draws().empty());
    session.set_view({160, 120}, {200, 150});
    session.update_time(0.02);
    EXPECT_EQ(session.gui().getContainer()->getSize(), (tgui::Vector2f{160, 120}));
    EXPECT_FLOAT_EQ(session.record().width(), 160);
    EXPECT_FLOAT_EQ(first.width(), 320); // Prior CPU recording remains independent.
    session.set_view({160, 120}, {0, 0});
    EXPECT_TRUE(session.record().draws().empty());
    session.set_view({0, 0}, {0, 0});
    EXPECT_TRUE(session.record().draws().empty());
    EXPECT_THROW(session.set_view({-1, 2}, {2, 2}), CE::Exceptions::invalid_args);
    EXPECT_THROW(session.update_time(-1), CE::Exceptions::invalid_args);
    EXPECT_THROW(session.update_time(std::numeric_limits<double>::infinity()), CE::Exceptions::invalid_args);
    EXPECT_THROW(session.gui().mainLoop(), CE::Exceptions::failed_operation);
}

TEST(ui_tgui_session, pointer_selection) {
    Input input;
    Session session(input, 7);
    session.set_view({320, 240}, {640, 480});
    const auto field = tgui::EditBox::create();
    field->setPosition({10, 10});
    field->setSize({200, 30});
    session.gui().add(field);
    const auto click = input.emit(
        ButtonEvent{800, ButtonPhase::Press, Modifiers::None, -1, -1, KeyboardKey::Unknown, MouseButton::Left, PointerEvent{20, 20}},
        DeviceKind::Mouse
    );
    session.handle_input(click->records, false);
    EXPECT_FALSE(field->isFocused());
    session.request_keyboard_focus();
    session.handle_input(click->records, true);
    EXPECT_TRUE(field->isFocused());
    const auto text = input.emit(TextEvent{U'a'});
    session.handle_input(text->records, true);
    EXPECT_EQ(field->getText(), "a");
}

TEST(ui_tgui_session, font_generations) {
    Input input;
    Session session(input, 7);
    session.set_view({320, 240}, {320, 240});
    const auto label = tgui::Label::create("A");
    label->setTextSize(32);
    session.gui().add(label);
    const auto first = session.record();
    std::shared_ptr<const CE::Assets::DecodedImage> atlas;
    for (const auto& draw : first.draws())
        if (draw.texture)
            atlas = draw.texture;
    ASSERT_TRUE(atlas);
    const auto pixels = atlas->rgba;
    const auto font = session.gui().getFont();
    ASSERT_TRUE(font.getBackendFont());
    EXPECT_TRUE(font.isSmooth());
    EXPECT_THROW(font.getBackendFont()->setSmooth(false), CE::Exceptions::invalid_args);
    EXPECT_TRUE(font.isSmooth());
    for (char32_t code = U' '; code < 0x300; ++code)
        if (font.getBackendFont()->hasGlyph(code))
            (void)font.getGlyph(code, 32, false);
    unsigned int version = 0;
    const auto texture = std::dynamic_pointer_cast<Texture>(font.getBackendFont()->getTexture(32, version));
    ASSERT_TRUE(texture);
    ASSERT_TRUE(texture->snapshot());
    EXPECT_NE(texture->snapshot(), atlas);
    EXPECT_GT(texture->snapshot()->rgba.size(), pixels.size());
    EXPECT_EQ(atlas->rgba, pixels);
}

TEST(ui_tgui_session, owner_teardown) {
    Input input;
    Session session(input, 7);
    session.set_view({320, 240}, {320, 240});
    session.gui().add(tgui::Label::create("Retained CPU scene"));
    const auto retained = session.record();
    std::thread other([&] { EXPECT_THROW(session.record(), CE::Exceptions::failed_operation); });
    other.join();
    // Serial transfer after the owner stops; no external toolkit references.
    std::thread teardown([&] { session.close_after_quiescence(); });
    teardown.join();
    EXPECT_FALSE(tgui::isBackendSet());
    EXPECT_FALSE(retained.draws().empty());
    for (const auto& draw : retained.draws())
        if (draw.texture)
            EXPECT_FALSE(draw.texture->rgba.empty());
    EXPECT_THROW(session.gui(), CE::Exceptions::failed_operation);
}

TEST(ui_tgui_session, prerequisites) {
    Input input;
    EXPECT_THROW(Session(input, 0), CE::Exceptions::invalid_args);
    EXPECT_THROW(Session(input, 7, {64, 1}), CE::Exceptions::invalid_args);
    EXPECT_THROW(Session(input, 7, {4096, 0}), CE::Exceptions::invalid_args);
    EXPECT_FALSE(tgui::isBackendSet());
    input.text_supported = false;
    input.focus_supported = false;
    Session session(input, 7);
    EXPECT_FALSE(session.capabilities().committed_text);
    EXPECT_FALSE(session.capabilities().keyboard_focus);
    EXPECT_THROW(session.request_keyboard_focus(), CE::Exceptions::failed_operation);
    EXPECT_FALSE(session.owns_keyboard_focus());
}
