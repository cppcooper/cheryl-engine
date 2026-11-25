#include "tgui-controls.h"
#include "ui-routing.h"

#include <ui/tgui/session.h>

#include <TGUI/Widgets/Button.hpp>
#include <TGUI/Widgets/EditBox.hpp>
#include <TGUI/Widgets/Label.hpp>
#include <TGUI/Widgets/Panel.hpp>
#include <TGUI/Widgets/Picture.hpp>
#include <TGUI/Widgets/ScrollablePanel.hpp>

#include <array>
#include <cmath>
#include <format>
#include <limits>

namespace {
    tgui::Texture image(const bool alternate) {
        constexpr unsigned int extent = 16;
        std::array<std::uint8_t, extent * extent * 4> pixels{};
        constexpr std::array<tgui::Color, 4> colors{tgui::Color{240, 90, 85}, tgui::Color{80, 210, 140}, tgui::Color{75, 145, 245},
                                                  tgui::Color{245, 205, 85}};
        for (unsigned int row = 0; row < extent; ++row)
            for (unsigned int column = 0; column < extent; ++column) {
                const auto color = colors[((column >= extent / 2) + 2 * (row >= extent / 2) + (alternate ? 1 : 0)) % colors.size()];
                const auto offset = (row * extent + column) * 4;
                pixels[offset] = color.getRed();
                pixels[offset + 1] = color.getGreen();
                pixels[offset + 2] = color.getBlue();
                pixels[offset + 3] = 255;
            }
        tgui::Texture texture;
        texture.loadFromPixelData({extent, extent}, pixels.data(), {}, {}, true);
        return texture;
    }

    tgui::Label::Ptr label(const tgui::String& text, const unsigned int size = 16) {
        auto widget = tgui::Label::create(text);
        widget->setTextSize(size);
        widget->getRenderer()->setTextColor({235, 240, 250});
        return widget;
    }
}

struct DemoTguiControls::State {
    CE::UI::TGUI::Session& session;
    std::shared_ptr<DemoUiRouting::Effects> effects = std::make_shared<DemoUiRouting::Effects>();
    tgui::Panel::Ptr panel;
    tgui::Label::Ptr status_label;
    tgui::EditBox::Ptr field;
    tgui::Picture::Ptr picture;
    tgui::Button::Ptr edge;
    bool visible = true;
    bool alternate_image = false;
    bool retired = false;

    explicit State(CE::UI::TGUI::Session& selected_session) : session(selected_session) {
        try {
            create_widgets();
        } catch (...) {
            retire();
            throw;
        }
    }
    ~State() { retire(); }

    void retire() {
        if (retired)
            return;
        if (panel || edge) {
            auto& gui = session.gui();
            effects.reset();
            if (panel)
                gui.remove(panel);
            if (edge)
                gui.remove(edge);
        } else
            effects.reset();
        status_label.reset();
        field.reset();
        picture.reset();
        panel.reset();
        edge.reset();
        visible = false;
        retired = true;
    }

    void create_widgets() {
        using CE::UI::TGUI::Scalable;
        auto& gui = session.gui();
        gui.setTextSize(16);
        gui.setKeyboardNavigationEnabled(true);
        panel = tgui::Panel::create({426, 500});
        panel->getRenderer()->setBackgroundColor({28, 36, 54, 225});
        panel->getRenderer()->setBorderColor({95, 125, 170});
        panel->getRenderer()->setBorders(1);
        gui.add(panel, "demo-panel");
        session.set_layout(
            panel, {.anchor = {1, 0}, .offset = {.fixed = {-24, 24}},
                    .scalable = Scalable{.width = 0.35f, .height = 0.75f, .min_width = 394, .max_width = 640,
                                         .min_height = 500, .max_height = 800}}
        );
        constexpr float side_margin = 18;
        constexpr float list_top = 270;
        constexpr float image_height = 68;
        constexpr float image_bottom = 14;
        constexpr float region_gap = 12;
        const auto content_width = tgui::bindInnerWidth(panel) - 2 * side_margin;
        const auto list_height = tgui::bindInnerHeight(panel) - list_top - region_gap - image_height - image_bottom;
        auto title = label("Cheryl UI", 24);
        panel->add(title);
        session.set_layout(title, {.offset = {.fixed = {18, 12}}});
        auto instructions = label("F2: edit   Esc: release focus   F3: hide/show");
        panel->add(instructions);
        session.set_layout(instructions, {.offset = {.fixed = {18, 48}}});
        status_label = label("");
        panel->add(status_label);
        session.set_layout(status_label, {.offset = {.fixed = {18, 80}}});
        const std::weak_ptr<DemoUiRouting::Effects> requests = effects;
        auto reset = tgui::Button::create("Reset camera");
        reset->setSize({180, 32});
        reset->onPress([requests] {
            if (const auto pending = requests.lock())
                pending->request_reset();
        });
        panel->add(reset, "reset");
        session.set_layout(reset, {.offset = {.fixed = {18, 148}}});
        field = tgui::EditBox::create();
        field->setSize({content_width, 34});
        field->setDefaultText("Click here and type");
        field->getRenderer()->setBackgroundColor({240, 244, 250});
        panel->add(field, "text-field");
        session.set_layout(field, {.offset = {.fixed = {side_margin, 198}}});
        auto editing = label("Arrows, Home/End and Backspace work here", 14);
        panel->add(editing);
        session.set_layout(editing, {.offset = {.fixed = {18, 240}}});
        auto scroll = tgui::ScrollablePanel::create({content_width, list_height});
        scroll->getRenderer()->setBackgroundColor({45, 60, 85, 180});
        scroll->setHorizontalScrollbarPolicy(tgui::Scrollbar::Policy::Never);
        panel->add(scroll, "scroll");
        session.set_layout(scroll, {.offset = {.fixed = {side_margin, list_top}}});
        const auto row_width = tgui::bindInnerWidth(scroll) - 16;
        for (unsigned int index = 0; index < 8; ++index) {
            auto row = tgui::Panel::create({row_width, 34});
            row->getRenderer()->setBackgroundColor({70, 105, 145, static_cast<std::uint8_t>(index % 2 ? 150 : 90)});
            scroll->add(row);
            session.set_layout(row, {.offset = {.fixed = {8, 8 + index * 38.0f}}});
            auto text = label(std::format("Scroll item {}", index + 1));
            row->add(text);
            session.set_layout(text, {.offset = {.fixed = {8, 6}}});
        }
        auto image_panel = tgui::Panel::create({content_width, image_height});
        image_panel->getRenderer()->setBackgroundColor({85, 75, 115, 130});
        panel->add(image_panel, "image-panel");
        session.set_layout(image_panel, {.anchor = {0, 1}, .offset = {.fixed = {side_margin, -image_bottom}}});
        picture = tgui::Picture::create(image(false));
        picture->setSize({52, 52});
        image_panel->add(picture, "image");
        session.set_layout(picture, {.offset = {.fixed = {8, 8}}});
        auto change = tgui::Button::create("Change image");
        change->setSize({180, 32});
        change->onPress([requests] {
            if (const auto pending = requests.lock())
                pending->request_image();
        });
        image_panel->add(change, "change-image");
        session.set_layout(change, {.offset = {.fixed = {82, 17}}});
        auto badge = tgui::Panel::create({100, 28});
        badge->getRenderer()->setBackgroundColor({80, 140, 170, 170});
        panel->add(badge);
        session.set_layout(badge, {.anchor = {1, 0}, .offset = {.fixed = {-8, 12}, .relative = {-0.02f, 0}}});
        auto badge_text = label("TGUI", 16);
        badge->add(badge_text);
        session.set_layout(badge_text, {.anchor = {0.5f, 0.5f}});
        edge = tgui::Button::create("?");
        edge->setSize({28, 28});
        auto tooltip = label("This tooltip stays inside the window.", 14);
        tooltip->getRenderer()->setBackgroundColor({30, 45, 65, 240});
        edge->setToolTip(tooltip);
        gui.add(edge, "edge-tooltip");
        session.set_layout(edge, {.anchor = {1, 1}, .offset = {.fixed = {-18, -18}}});
    }
};

DemoTguiControls::DemoTguiControls(CE::UI::TGUI::Session& session) : state_(std::make_unique<State>(session)) {}
DemoTguiControls::~DemoTguiControls() = default;
void DemoTguiControls::retire() { state_->retire(); }
CE::UI::TGUI::Session& DemoTguiControls::session() const { return state_->session; }
bool DemoTguiControls::visible() const noexcept { return state_->visible; }
void DemoTguiControls::set_visible(bool visible) {
    if (state_->retired)
        return;
    state_->visible = visible;
    state_->panel->setVisible(visible);
    state_->edge->setVisible(visible);
}
bool DemoTguiControls::contains(const CE::Input::Events::Pointer& position) const {
    if (state_->retired || !std::isfinite(position.x) || !std::isfinite(position.y) || position.x < std::numeric_limits<int>::min() ||
        position.x > std::numeric_limits<int>::max() || position.y < std::numeric_limits<int>::min() ||
        position.y > std::numeric_limits<int>::max())
        return false;
    const auto point = session().gui().mapPixelToCoords({static_cast<int>(position.x), static_cast<int>(position.y)});
    const auto inside = [&](const tgui::Widget::Ptr& widget) {
        const auto origin = widget->getPosition();
        const auto size = widget->getSize();
        return point.x >= origin.x && point.y >= origin.y && point.x < origin.x + size.x && point.y < origin.y + size.y;
    };
    return inside(state_->panel) || inside(state_->edge);
}
bool DemoTguiControls::field_contains(const CE::Input::Events::Pointer& position) const {
    if (!contains(position))
        return false;
    const auto point = session().gui().mapPixelToCoords({static_cast<int>(position.x), static_cast<int>(position.y)});
    const auto origin = state_->field->getAbsolutePosition();
    const auto size = state_->field->getSize();
    return point.x >= origin.x && point.y >= origin.y && point.x < origin.x + size.x && point.y < origin.y + size.y;
}
void DemoTguiControls::focus_field() {
    if (!state_->retired)
        state_->field->setFocused(true);
}
void DemoTguiControls::update_status(const DemoUiStatus& status) {
    if (state_->retired)
        return;
    state_->status_label->setText(std::format(
        "Updates: {}   Camera: {:.0f}, {:.0f}\nClicks: {}   Wheel: {:.2f}\nGamepad A: {} presses", status.updates, status.pan_x,
        status.pan_y, status.clicks, status.wheel, status.gamepad_presses
    ));
}
void DemoTguiControls::begin_input(bool enabled) {
    if (state_->effects)
        state_->effects->begin(enabled);
}
void DemoTguiControls::select_effects(bool selected) {
    if (state_->effects)
        state_->effects->select(selected);
}
void DemoTguiControls::cancel_effects() {
    if (state_->effects)
        state_->effects->cancel();
}
void DemoTguiControls::finish_input() {
    if (state_->effects && state_->effects->finish_image()) {
        const bool alternate = !state_->alternate_image;
        state_->picture->getRenderer()->setTexture(image(alternate));
        state_->alternate_image = alternate;
    }
}
bool DemoTguiControls::consume_reset() noexcept { return state_->effects && state_->effects->consume_reset(); }
bool DemoTguiControls::alternate_image() const noexcept { return state_->alternate_image; }
void DemoTguiControls::handle_input(std::span<const CE::Input::Events::Record> records, bool drawable) {
    using namespace CE::Input;
    if (state_->retired)
        return;
    begin_input(drawable && visible());
    session().handle_input(records, drawable && visible(), [&](const Events::Record& record) {
        const auto* button = std::get_if<Events::Button>(&record.data);
        if (button && record.device_kind == DeviceKind::Keyboard && button->phase == Events::ButtonPhase::Press) {
            if (button->key == KeyboardKey::F2 && visible() && drawable) {
                if (session().owns_keyboard_focus())
                    session().release_keyboard_focus();
                else {
                    session().request_keyboard_focus();
                    focus_field();
                }
            } else if (button->key == KeyboardKey::F3) {
                set_visible(!visible());
                if (!visible())
                    session().release_keyboard_focus();
            } else if (button->key == KeyboardKey::Escape)
                session().release_keyboard_focus();
        }
        if (drawable && visible() && button && record.device_kind == DeviceKind::Mouse &&
            button->mouse_button == MouseButton::Left && button->phase == Events::ButtonPhase::Press && button->position) {
            if (contains(*button->position)) {
                if (!session().owns_keyboard_focus())
                    session().request_keyboard_focus();
            } else
                session().release_keyboard_focus();
        }
        select_effects(drawable && visible());
        return drawable && visible();
    });
    finish_input();
}
