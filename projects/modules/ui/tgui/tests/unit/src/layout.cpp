#include <gtest/gtest.h>

#include <ui/tgui/session.h>
#include <internals/exceptions.h>

#include <TGUI/Widgets/Label.hpp>
#include <TGUI/Widgets/Panel.hpp>

#include <limits>
#include <memory>
#include <thread>
#include <vector>

namespace {
    using namespace CE::UI::TGUI;

    class Input final : public CE::Input::iInputSystem {
        CE::Input::InputBindings bindings_;

    public:
        void initialize(CE::iWindow&) override {}
        void poll() override {
            begin_input_poll();
            static_cast<void>(publish_input());
        }
        void deinitialize() override {}
        CE::Input::InputBindings& bindings() override { return bindings_; }
        CE::Input::DeviceId keyboard_id() const override { return 1; }
        CE::Input::DeviceId mouse_id() const override { return 2; }
        CE::Input::DeviceId gamepad_id() const override { return 3; }
        bool supports(const CE::Input::InputMode mode) const override {
            return mode == CE::Input::InputMode::State || mode == CE::Input::InputMode::Events;
        }
    };
}

TEST(ui_tgui_layout, anchor) {
    Input input;
    Session session(input, 7);
    session.set_view({320, 240}, {640, 480});
    const auto badge = tgui::Panel::create({100, 28});
    session.gui().add(badge);
    session.set_layout(badge, {.anchor = {1, 0}, .offset = {.fixed = {-16, 12}}});
    EXPECT_EQ(badge->getOrigin(), (tgui::Vector2f{1, 0}));
    EXPECT_EQ(badge->getAbsolutePosition(), (tgui::Vector2f{204, 12}));
    EXPECT_EQ(badge->getSize(), (tgui::Vector2f{100, 28}));
    session.set_view({640, 480}, {800, 600});
    EXPECT_EQ(badge->getAbsolutePosition(), (tgui::Vector2f{524, 12}));
    EXPECT_EQ(badge->getSize(), (tgui::Vector2f{100, 28}));
}

TEST(ui_tgui_layout, offsets) {
    Input input;
    Session session(input, 7);
    session.set_view({640, 480}, {640, 480});
    const auto parent = tgui::Panel::create({200, 120});
    parent->setPosition({20, 30});
    parent->getRenderer()->setBorders(2);
    parent->getRenderer()->setPadding(8);
    session.gui().add(parent);
    const auto child = tgui::Panel::create({20, 10});
    parent->add(child);
    session.set_layout(
        child, {.anchor = {0.5f, 0.5f}, .origin = tgui::Vector2f{0, 0}, .offset = {.fixed = {5, 10}, .relative = {-0.1f, 0.2f}}}
    );
    EXPECT_EQ(parent->getInnerSize(), (tgui::Vector2f{180, 100}));
    EXPECT_EQ(child->getPosition(), (tgui::Vector2f{77, 80}));
    EXPECT_EQ(child->getAbsolutePosition(), (tgui::Vector2f{107, 120}));
    parent->setSize({400, 220});
    EXPECT_EQ(child->getPosition(), (tgui::Vector2f{157, 150}));
    EXPECT_EQ(child->getAbsolutePosition(), (tgui::Vector2f{187, 190}));
    parent->getRenderer()->setPadding(10);
    EXPECT_FLOAT_EQ(child->getPosition().x, 155.4f);
    EXPECT_FLOAT_EQ(child->getPosition().y, 147.2f);
    parent->getRenderer()->setBorders(4);
    EXPECT_FLOAT_EQ(child->getPosition().x, 153.8f);
    EXPECT_FLOAT_EQ(child->getPosition().y, 144.4f);
}

TEST(ui_tgui_layout, scalable) {
    Input input;
    Session session(input, 7);
    session.set_view({1000, 800}, {1000, 800});
    const auto panel = tgui::Panel::create({100, 50});
    session.gui().add(panel);
    session.set_layout(
        panel,
        {.anchor = {1, 1},
         .scalable = Scalable{.width = 0.5f, .height = 0.25f, .min_width = 200, .max_width = 400, .min_height = 100, .max_height = 300}}
    );
    EXPECT_EQ(panel->getSize(), (tgui::Vector2f{400, 200}));
    EXPECT_EQ(panel->getAbsolutePosition(), (tgui::Vector2f{600, 600}));
    session.set_view({400, 240}, {800, 480});
    EXPECT_EQ(panel->getSize(), (tgui::Vector2f{200, 100}));
    session.set_view({100, 80}, {100, 80});
    EXPECT_EQ(panel->getSize(), (tgui::Vector2f{200, 100})); // Minimums survive a smaller parent.
    session.set_view({1600, 1600}, {1600, 1600});
    EXPECT_EQ(panel->getSize(), (tgui::Vector2f{400, 300}));
}

TEST(ui_tgui_layout, replace) {
    Input input;
    Session session(input, 7);
    session.set_view({320, 240}, {320, 240});
    const auto panel = tgui::Panel::create({100, 30});
    session.gui().add(panel);
    session.set_layout(panel, {.scalable = Scalable{.width = 0.5f}});
    EXPECT_EQ(panel->getSize(), (tgui::Vector2f{160, 30}));
    session.set_view({640, 480}, {640, 480});
    EXPECT_EQ(panel->getSize(), (tgui::Vector2f{320, 30}));
    session.set_layout(panel, {.scalable = Scalable{.height = 0.25f, .min_height = 60, .max_height = 120}});
    EXPECT_EQ(panel->getSize(), (tgui::Vector2f{320, 120}));
    session.set_view({1280, 240}, {1280, 240});
    EXPECT_EQ(panel->getSize(), (tgui::Vector2f{320, 60}));
    session.set_layout(panel, {});
    session.set_view({1600, 800}, {1600, 800});
    EXPECT_EQ(panel->getSize(), (tgui::Vector2f{320, 60}));
}

TEST(ui_tgui_layout, native_size) {
    Input input;
    Session session(input, 7);
    session.set_view({320, 240}, {320, 240});
    const auto label = tgui::Label::create("A");
    session.gui().add(label);
    session.set_layout(label, {.anchor = {0.5f, 0.5f}});
    const auto first_width = label->getSize().x;
    label->setText("Native automatic sizing remains active");
    EXPECT_GT(label->getSize().x, first_width);
    EXPECT_FLOAT_EQ(label->getAbsolutePosition().x + label->getSize().x / 2, 160);

    const auto source = tgui::Panel::create({100, 20});
    const auto panel = tgui::Panel::create({100, 30});
    session.gui().add(source);
    session.gui().add(panel);
    panel->setSize({100, tgui::bindHeight(source)});
    session.set_layout(panel, {.scalable = Scalable{.width = 0.5f}});
    source->setSize({100, 50});
    EXPECT_EQ(panel->getSize(), (tgui::Vector2f{160, 50}));
    session.set_layout(panel, {});
    session.set_view({640, 480}, {640, 480});
    source->setSize({100, 70});
    EXPECT_EQ(panel->getSize(), (tgui::Vector2f{160, 70}));
}

TEST(ui_tgui_layout, ownership) {
    Input input;
    Session session(input, 7);
    session.set_view({320, 240}, {320, 240});
    const auto child = tgui::Panel::create({20, 20});
    EXPECT_THROW(session.set_layout(nullptr, {}), CE::Exceptions::invalid_args);
    EXPECT_THROW(session.set_layout(child, {}), CE::Exceptions::invalid_args);
    const auto foreign = tgui::Panel::create({100, 100});
    foreign->add(child);
    EXPECT_THROW(session.set_layout(child, {}), CE::Exceptions::invalid_args);
    foreign->remove(child);
    const auto first = tgui::Panel::create({100, 100});
    const auto second = tgui::Panel::create({240, 100});
    session.gui().add(first);
    session.gui().add(second);
    first->add(child);
    const WidgetLayout layout{.anchor = {1, 1}, .offset = {.fixed = {-10, -10}}};
    session.set_layout(child, layout);
    EXPECT_EQ(child->getPosition(), (tgui::Vector2f{90, 90}));
    first->remove(child);
    second->add(child);
    session.set_layout(child, layout); // Reparenting explicitly replaces the binding.
    first->setSize({400, 400});
    EXPECT_EQ(child->getPosition(), (tgui::Vector2f{230, 90}));
    second->setSize({300, 200});
    EXPECT_EQ(child->getPosition(), (tgui::Vector2f{290, 190}));
    std::thread other([&] { EXPECT_THROW(session.set_layout(child, layout), CE::Exceptions::failed_operation); });
    other.join();
    child->setAutoLayout(tgui::AutoLayout::Top);
    EXPECT_THROW(session.set_layout(child, layout), CE::Exceptions::invalid_args);
}

TEST(ui_tgui_layout, lifetime) {
    Input input;
    Session session(input, 7);
    session.set_view({320, 240}, {320, 240});
    auto panel = tgui::Panel::create({100, 30});
    const std::weak_ptr<tgui::Widget> saved = panel;
    session.gui().add(panel);
    panel->onSizeChange([&] {
        session.gui().remove(panel);
        panel.reset();
    });
    session.set_layout(panel, {.scalable = Scalable{.width = 0.5f}});
    EXPECT_FALSE(panel); // A native size callback can release the caller's handle.
    EXPECT_TRUE(saved.expired());
    session.set_view({640, 480}, {640, 480});
    const auto next = tgui::Panel::create({20, 30});
    session.gui().add(next);
    session.set_layout(next, {.anchor = {1, 0}});
    EXPECT_EQ(next->getSize(), (tgui::Vector2f{20, 30}));
    EXPECT_EQ(next->getAbsolutePosition(), (tgui::Vector2f{620, 0}));
}

TEST(ui_tgui_layout, invalid) {
    Input input;
    Session session(input, 7);
    session.set_view({320, 240}, {320, 240});
    const auto panel = tgui::Panel::create({100, 30});
    panel->setPosition({10, 20});
    panel->setOrigin({0.25f, 0.25f});
    session.gui().add(panel);
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const std::vector<WidgetLayout> invalid{
        {.anchor = {1.1f, 0}},
        {.origin = tgui::Vector2f{0, -0.1f}},
        {.offset = {.relative = {nan, 0}}},
        {.scalable = Scalable{}},
        {.scalable = Scalable{.width = -0.1f}},
        {.scalable = Scalable{.width = nan}},
        {.scalable = Scalable{.width = 0.5f, .min_width = 300, .max_width = 200}},
        {.scalable = Scalable{.height = 0.5f, .min_height = std::numeric_limits<float>::infinity()}},
    };
    for (const auto& layout : invalid) {
        EXPECT_THROW(session.set_layout(panel, layout), CE::Exceptions::invalid_args);
        EXPECT_EQ(panel->getPosition(), (tgui::Vector2f{10, 20}));
        EXPECT_EQ(panel->getOrigin(), (tgui::Vector2f{0.25f, 0.25f}));
        EXPECT_EQ(panel->getSize(), (tgui::Vector2f{100, 30}));
    }
}

TEST(ui_tgui_layout, recording) {
    Input input;
    Session session(input, 7);
    session.set_view({100, 100}, {200, 200});
    const auto panel = tgui::Panel::create({20, 10});
    panel->getRenderer()->setBorders(0);
    panel->getRenderer()->setBackgroundColor(tgui::Color::Red);
    session.gui().add(panel);
    session.set_layout(panel, {.anchor = {1, 1}});
    const auto recorded = session.record();
    ASSERT_EQ(recorded.draws().size(), 1u);
    ASSERT_FALSE(recorded.draws()[0].vertices.empty());
    for (const auto& vertex : recorded.draws()[0].vertices) {
        EXPECT_GE(vertex.x, 80);
        EXPECT_LE(vertex.x, 100);
        EXPECT_GE(vertex.y, 90);
        EXPECT_LE(vertex.y, 100);
    }
    EXPECT_EQ(panel->getSize(), (tgui::Vector2f{20, 10}));
}
