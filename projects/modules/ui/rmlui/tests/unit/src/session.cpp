#include <ui/rmlui/session.h>
#include <text/font-rasterizer.h>
#include <text/font-service.h>
#include <internals/exceptions.h>
#include <testing/font-fixture.h>
#include "../../support/document.h"

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/FontEngineInterface.h>
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

    class Input final : public CE::Input::System {
        CE::Input::Actions::Bindings bindings_;

    public:
        bool text_supported = true;
        bool focus_supported = true;

        void initialize(CE::Platform::Window&) override {}
        void poll() override {}
        void deinitialize() override {}
        CE::Input::Actions::Bindings& bindings() override { return bindings_; }
        DeviceId keyboard_id() const override { return 1; }
        DeviceId mouse_id() const override { return 2; }
        DeviceId gamepad_id() const override { return 3; }
        bool supports(const CE::Input::Capture::Channel mode) const override {
            return mode != CE::Input::Capture::Channel::Text || text_supported;
        }
        bool supports_focus() const override { return focus_supported; }
        std::shared_ptr<const PollSnapshot> emit(CE::Input::Events::Payload data, const DeviceKind kind = DeviceKind::Keyboard) {
            begin_input_poll();
            capture_buffer().record(kind == DeviceKind::Mouse ? mouse_id() : keyboard_id(), kind, std::move(data));
            return publish_input();
        }
    };

    CE::Input::Events::Button
    key(const KeyboardKey key,
        const Modifiers flags = Modifiers::None,
        const CE::Input::Events::ButtonPhase phase = CE::Input::Events::ButtonPhase::Press) {
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
        const auto text = input.emit(CE::Input::Events::Text{U'a'});
        ASSERT_EQ(text->records.size(), 1u);
        EXPECT_EQ(text->records[0].target, 7u);
        session.release_keyboard_focus();
        EXPECT_TRUE(input.emit(CE::Input::Events::Text{U'a'})->records.empty());
    }
    EXPECT_EQ(Rml::GetSystemInterface(), nullptr);
    EXPECT_TRUE(input.emit(key(KeyboardKey::A))->records.empty());
    Session next(input, 9, {});
    EXPECT_NE(Rml::GetSystemInterface(), nullptr);
}

TEST(ui_rmlui_session, engine_font) {
    Input input;
    Session session(input, 7);
    std::ifstream file(CHERYL_RMLUI_TEST_FONT, std::ios::binary);
    std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    ASSERT_FALSE(bytes.empty());
    const auto face = CE::Text::FontRasterizer::load(bytes);
    const auto expected = face.metrics({24, CE::Text::FontSizing::Characters});
    const auto glyph = face.rasterize(U'A', {24, CE::Text::FontSizing::Characters}, {.color = true});
    ASSERT_TRUE(expected);
    ASSERT_TRUE(glyph);
    ASSERT_TRUE(session.load_font(bytes, "engine-font"));
    std::fill(bytes.begin(), bytes.end(), 0);
    auto* fonts = Rml::GetFontEngineInterface();
    ASSERT_NE(fonts, nullptr);
    const auto handle = fonts->GetFontFaceHandle("engine-font", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, 24);
    ASSERT_NE(handle, 0u);
    const auto& metrics = fonts->GetFontMetrics(handle);
    EXPECT_EQ(metrics.ascent, expected->ascent);
    EXPECT_EQ(metrics.descent, -expected->descent);
    EXPECT_EQ(metrics.line_spacing, expected->line_spacing);
    EXPECT_EQ(metrics.x_height, expected->x_height);
    const Rml::String language;
    EXPECT_EQ(fonts->GetStringWidth(handle, "A", {language}), static_cast<int>(glyph->advance));
}

TEST(ui_rmlui_session, font_catalogue) {
    Input input;
    CE::Testing::FontFixture files;
    const auto regular = files.path / "regular.ttf";
    const auto bold = files.path / "bold.ttf";
    const auto italic = files.path / "italic.ttf";
    CE::Testing::styled_file(regular, 400, false);
    CE::Testing::styled_file(bold, 700, false);
    CE::Testing::styled_file(italic, 400, true);
    auto fonts = std::make_unique<CE::Text::FontService>();
    CE::Text::FontConfiguration configuration;
    configuration.families = {
        {"book-print", {CE::Text::FontFile{regular}}},
        {"handwriting", {CE::Text::FontFile{regular}, CE::Text::FontFile{bold}, CE::Text::FontFile{italic}}}
    };
    configuration.default_font = "book-print";
    configuration.asset_directories.clear();
    configuration.system_directories = std::vector<std::filesystem::path>{};
    fonts->configure(configuration);
    const auto chosen = fonts->get("handwriting");
    std::filesystem::remove(regular);
    std::filesystem::remove(bold);
    std::filesystem::remove(italic);
    Session session(input, *fonts, 7);
    fonts.reset();
    session.set_view({320, 240}, {320, 240});
    auto* document = session.context().LoadDocumentFromMemory(
        "<rml><head><style>body { font-size: 24px; }</style></head><body><div id='text'>A book</div></body></rml>"
    );
    ASSERT_NE(document, nullptr);
    document->Show();
    session.update_time(0);
    auto* child = document->GetElementById("text");
    ASSERT_NE(child, nullptr);
    EXPECT_EQ(child->GetProperty<Rml::String>("font-family"), "book-print");
    EXPECT_FALSE(session.record().draws().empty());
    EXPECT_EQ(session.font_family(chosen), "handwriting");
    session.set_font(*document, chosen);
    session.update_time(0);
    EXPECT_EQ(child->GetProperty<Rml::String>("font-family"), "handwriting");
    auto* engine = Rml::GetFontEngineInterface();
    const auto normal = engine->GetFontFaceHandle("handwriting", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, 24);
    const auto heavy = engine->GetFontFaceHandle("handwriting", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Bold, 24);
    const auto slanted = engine->GetFontFaceHandle("handwriting", Rml::Style::FontStyle::Italic, Rml::Style::FontWeight::Normal, 24);
    ASSERT_NE(normal, 0u);
    ASSERT_NE(heavy, 0u);
    ASSERT_NE(slanted, 0u);
    EXPECT_NE(heavy, normal);
    EXPECT_NE(slanted, normal);
    EXPECT_THROW(static_cast<void>(session.load_font(chosen.faces().front().bytes, "HANDWRITING")), CE::Exceptions::invalid_args);
    EXPECT_THROW(static_cast<void>(session.font_family({})), CE::Exceptions::invalid_args);
}

TEST(ui_rmlui_session, embedded_default) {
    Input input;
    CE::Text::FontService fonts;
    Session session(input, fonts, 7);
    session.set_view({320, 240}, {320, 240});
    auto* document = session.context().LoadDocumentFromMemory("<rml><body>Engine default without registration</body></rml>");
    ASSERT_NE(document, nullptr);
    document->Show();
    EXPECT_FALSE(session.record().draws().empty());
    EXPECT_EQ(document->GetProperty<Rml::String>("font-family"), fonts.default_font().reference());
}

TEST(ui_rmlui_session, font_index) {
    Input input;
    CE::Testing::FontFixture files;
    const auto path = files.path / "book.ttc";
    CE::Testing::collection_file(path);
    CE::Text::FontService fonts;
    CE::Text::FontConfiguration configuration;
    configuration.families = {{"book-print", {CE::Text::FontFile{path, 1}}}};
    configuration.default_font = "book-print";
    configuration.asset_directories.clear();
    configuration.system_directories = std::vector<std::filesystem::path>{};
    fonts.configure(configuration);
    const auto selected = fonts.default_font();
    ASSERT_EQ(selected.reference(), "book-print");
    ASSERT_EQ(selected.faces().front().face_index, 1u);
    const auto& source = selected.faces().front();
    const auto expected = CE::Text::FontRasterizer::load(source.bytes, 1).metrics({24, CE::Text::FontSizing::Characters});
    const auto other = CE::Text::FontRasterizer::load(source.bytes, 0).metrics({24, CE::Text::FontSizing::Characters});
    ASSERT_TRUE(expected);
    ASSERT_TRUE(other);
    ASSERT_NE(expected->ascent, other->ascent);
    Session session(input, fonts, 7);
    const auto handle = Rml::GetFontEngineInterface()->GetFontFaceHandle(
        "book-print", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Bold, 24
    );
    ASSERT_NE(handle, 0u);
    EXPECT_EQ(Rml::GetFontEngineInterface()->GetFontMetrics(handle).ascent, expected->ascent);
}

TEST(ui_rmlui_session, font_aliases) {
    Input input;
    CE::Text::FontService fonts;
    CE::Text::FontConfiguration configuration;
    configuration.families = {
        {"book-print", {CE::Text::FontFile{CE::Testing::font_fixture_source}}},
        {"BOOK-PRINT", {CE::Text::FontFile{CE::Testing::font_fixture_source}}}
    };
    configuration.asset_directories.clear();
    configuration.system_directories = std::vector<std::filesystem::path>{};
    fonts.configure(configuration);
    EXPECT_THROW(Session(input, fonts, 7), CE::Exceptions::invalid_args);
    EXPECT_EQ(Rml::GetSystemInterface(), nullptr);
    Session standalone(input, 7);
    ASSERT_TRUE(standalone.load_font(CE::Testing::font_fixture_source, "book-print"));
    EXPECT_THROW(static_cast<void>(standalone.font_family(fonts.get("book-print"))), CE::Exceptions::invalid_args);
}

TEST(ui_rmlui_session, missing_style) {
    Input input;
    CE::Testing::FontFixture files;
    for (const bool italic : {false, true}) {
        const auto path = files.path / (italic ? "italic.ttf" : "regular.ttf");
        CE::Testing::styled_file(path, 400, italic);
        CE::Text::FontService fonts;
        CE::Text::FontConfiguration configuration;
        configuration.families = {{"book-print", {CE::Text::FontFile{path}}}};
        configuration.default_font = "book-print";
        configuration.fallbacks = {"book-print"};
        configuration.asset_directories.clear();
        configuration.system_directories = std::vector<std::filesystem::path>{};
        fonts.configure(configuration);
        Session session(input, fonts, 7);
        auto* engine = Rml::GetFontEngineInterface();
        EXPECT_NE(engine->GetFontFaceHandle("book-print", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, 24), 0u);
        EXPECT_NE(engine->GetFontFaceHandle("book-print", Rml::Style::FontStyle::Italic, Rml::Style::FontWeight::Normal, 24), 0u);
        session.set_view({320, 240}, {320, 240});
        auto* document = session.context().LoadDocumentFromMemory(
            "<rml><head><style>body { font-size: 24px; } .slanted { font-style: italic; }</style></head>"
            "<body>Normal request<div class='slanted'>Italic request</div></body></rml>"
        );
        ASSERT_NE(document, nullptr);
        document->Show();
        EXPECT_FALSE(session.record().draws().empty());
    }
}

TEST(ui_rmlui_session, fallback_names) {
    Input input;
    CE::Text::FontService fonts;
    CE::Text::FontConfiguration configuration;
    configuration.families = {{"__cheryl_fallback_0", {CE::Text::FontFile{CE::Testing::font_fixture_source}}}};
    configuration.asset_directories.clear();
    configuration.system_directories = std::vector<std::filesystem::path>{};
    fonts.configure(configuration);
    const auto selected = fonts.get("__cheryl_fallback_0");
    ASSERT_EQ(selected.reference(), "__cheryl_fallback_0");
    {
        Session session(input, 7);
        EXPECT_EQ(session.font_family(selected), selected.reference());
        EXPECT_NE(Rml::GetFontEngineInterface()->GetFontFaceHandle(
                      selected.reference(), Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, 24
                  ),
                  0u);
    }
    {
        Session session(input, 7);
        ASSERT_TRUE(session.load_font(CE::Testing::font_fixture_source, "__cheryl_fallback_0"));
        const auto fallback = fonts.default_font();
        EXPECT_EQ(session.font_family(fallback), fallback.reference());
        EXPECT_THROW(static_cast<void>(session.font_family(selected)), CE::Exceptions::invalid_args);
    }
}

TEST(ui_rmlui_session, routing) {
    Input input;
    Session session(input, 7);
    auto& document = RmlUiTests::document(session);
    auto& field = RmlUiTests::field(document);
    session.request_keyboard_focus();
    ASSERT_TRUE(field.Focus());
    const auto first = input.emit(CE::Input::Events::Text{U'a'});
    session.handle_input(first->records, false);
    EXPECT_EQ(field.GetValue(), "a");
    auto foreign = first->records;
    foreign[0].target = 9;
    session.handle_input(foreign, false);
    EXPECT_EQ(field.GetValue(), "a");
    session.request_keyboard_focus();
    session.handle_input(first->records, false);
    EXPECT_EQ(field.GetValue(), "a");
    const auto pending = input.emit(CE::Input::Events::Text{U'b'});
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
        session.handle_input(input.emit(CE::Input::Events::Text{character})->records, false);
    EXPECT_EQ(
        field.GetValue(), "a\xc3\xa9"
                          "b"
    );
    session.handle_input(input.emit(key(KeyboardKey::Backspace, Modifiers::None, CE::Input::Events::ButtonPhase::Repeat))->records, false);
    EXPECT_EQ(field.GetValue(), "a\xc3\xa9");
    session.handle_input(input.emit(key(KeyboardKey::Left, Modifiers::Shift))->records, false);
    session.handle_input(input.emit(CE::Input::Events::Text{U'x'})->records, false);
    EXPECT_EQ(field.GetValue(), "ax");
    field.SetValue("keep");
    field.Select();
    for (const auto shortcut : {KeyboardKey::C, KeyboardKey::X, KeyboardKey::V}) {
        session.handle_input(input.emit(key(shortcut, Modifiers::Control))->records, false);
        EXPECT_EQ(field.GetValue(), "keep");
    }
    EXPECT_THROW(Rml::GetSystemInterface()->SetClipboardText("private"), CE::Exceptions::failed_operation);
    EXPECT_THROW(session.handle_input(input.emit(CE::Input::Events::Text{0xD800})->records, false), CE::Exceptions::invalid_args);
}

TEST(ui_rmlui_session, click_position) {
    Input input;
    Session session(input, 7);
    auto& document = RmlUiTests::document(session);
    auto& field = RmlUiTests::field(document);
    field.Blur();
    const CE::Input::Events::Button click{
        900,
        CE::Input::Events::ButtonPhase::Press,
        Modifiers::None,
        -1,
        -1,
        KeyboardKey::Unknown,
        MouseButton::Left,
        CE::Input::Events::Pointer{20, 45}
    };
    session.handle_input(input.emit(click, DeviceKind::Mouse)->records, false);
    EXPECT_NE(session.context().GetFocusElement(), &field);
    // A first click carries its own position; no earlier movement is necessary.
    session.handle_input(input.emit(click, DeviceKind::Mouse)->records, true);
    EXPECT_EQ(session.context().GetFocusElement(), &field);
    auto release = click;
    release.phase = CE::Input::Events::ButtonPhase::Release;
    session.handle_input(input.emit(release, DeviceKind::Mouse)->records, true);
    session.handle_input(input.emit(CE::Input::Events::Pointer{300, 200}, DeviceKind::Mouse)->records, true);
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
    const auto poll = input.emit(CE::Input::Events::Scroll{0.5, -0.25, CE::Input::Events::Pointer{20.75, 45.25}}, DeviceKind::Mouse);
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
