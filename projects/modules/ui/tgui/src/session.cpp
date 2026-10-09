#include <ui/tgui/session.h>

#include "dependency-contract.h"
#include <ui/tgui/input.h>
#include <internals/exceptions.h>

#include <TGUI/Backend/Font/FreeType/BackendFontFreeType.hpp>
#include <TGUI/Backend/Window/Backend.hpp>
#include <TGUI/Container.hpp>
#include <TGUI/Keyboard.hpp>

#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace CE::UI::TGUI {
    namespace {
        std::mutex backend_mutex;

        class Backend final : public tgui::Backend {
        public:
            void setMouseCursorStyle(tgui::Cursor::Type, const std::uint8_t*, tgui::Vector2u, tgui::Vector2u) override {}
            void resetMouseCursorStyle(tgui::Cursor::Type) override {}
            void setMouseCursor(tgui::BackendGui*, tgui::Cursor::Type) override {}
            void setClipboard(const tgui::String&) override {
                throw Exceptions::failed_operation(CE_HERE, "TGUI OS clipboard service is unavailable");
            }
            [[nodiscard]] tgui::String getClipboard() const override {
                throw Exceptions::failed_operation(CE_HERE, "TGUI OS clipboard service is unavailable");
            }
        };

        class BackendLease final {
            std::shared_ptr<Backend> backend_;

        public:
            explicit BackendLease(const SessionOptions options) {
                if (options.maximum_texture_size < 128 || !std::isfinite(options.font_scale) || options.font_scale <= 0)
                    throw Exceptions::invalid_args(CE_HERE, "TGUI needs a texture bound of at least 128 and a positive finite font scale");
                const std::lock_guard lock(backend_mutex);
                if (tgui::isBackendSet())
                    throw Exceptions::failed_operation(CE_HERE, "TGUI already has an active backend/session");
                backend_ = std::make_shared<Backend>();
                backend_->setRenderer(std::make_shared<Renderer>(options.maximum_texture_size));
                backend_->setFontBackend(std::make_shared<tgui::BackendFontFactoryImpl<tgui::BackendFontFreeType>>());
                backend_->setFontScale(options.font_scale);
                tgui::setBackend(backend_);
            }

            ~BackendLease() {
                const std::lock_guard lock(backend_mutex);
                // GUI and all application-held toolkit resources are gone first.
                // TGUI clears its global font, theme and timers while backend lives.
                tgui::setBackend(nullptr);
            }
        };

        class Gui final : public tgui::BackendGui {
            Input::Modifiers modifiers_ = Input::Modifiers::None;

        public:
            explicit Gui(const std::shared_ptr<RenderTarget>& target) {
                m_backendRenderTarget = target;
                setDrawingUpdatesTime(false);
                updateContainerSize();
                tgui::getBackend()->attachGui(this);
            }

            void set_logical_size(const ViewPort<int> size) {
                // TGUI's mapping uses this member for input and layout. Its units
                // are deliberately logical here; physical rounding lives in target.
                m_framebufferSize = {size.width, size.height};
                updateContainerSize();
            }
            void set_modifiers(const Input::Modifiers modifiers) { modifiers_ = modifiers; }

            [[nodiscard]] bool isKeyboardModifierPressed(const tgui::Event::KeyModifier modifier) const override {
                switch (modifier) {
                    case tgui::Event::KeyModifier::System:
                        return Input::has_modifier(modifiers_, Input::Modifiers::Super);
                    case tgui::Event::KeyModifier::Control:
                        return Input::has_modifier(modifiers_, Input::Modifiers::Control);
                    case tgui::Event::KeyModifier::Shift:
                        return Input::has_modifier(modifiers_, Input::Modifiers::Shift);
                    case tgui::Event::KeyModifier::Alt:
                        return Input::has_modifier(modifiers_, Input::Modifiers::Alt);
                }
                return false;
            }

            void mainLoop(tgui::Color) override {
                throw Exceptions::failed_operation(CE_HERE, "TGUI is driven by Cheryl's runtime, not a toolkit main loop");
            }
        };

        bool normalized(const tgui::Vector2f point) {
            return std::isfinite(point.x) && std::isfinite(point.y) && point.x >= 0 && point.x <= 1 && point.y >= 0 && point.y <= 1;
        }

        void validate_axis(const std::optional<float> fraction, const float minimum, const float maximum) {
            if (fraction && (!std::isfinite(*fraction) || *fraction < 0 || *fraction > 1))
                throw Exceptions::invalid_args(CE_HERE, "TGUI scalable fractions must be finite values in [0, 1]");
            if (!std::isfinite(minimum) || minimum < 0 || std::isnan(maximum) || maximum < minimum)
                throw Exceptions::invalid_args(CE_HERE, "TGUI scalable bounds require a finite nonnegative minimum and maximum >= minimum");
        }

        void validate_layout(const WidgetLayout& layout) {
            if (!normalized(layout.anchor) || (layout.origin && !normalized(*layout.origin)))
                throw Exceptions::invalid_args(CE_HERE, "TGUI layout anchors and origins must be finite values in [0, 1]");
            if (!std::isfinite(layout.offset.fixed.x) || !std::isfinite(layout.offset.fixed.y) ||
                !std::isfinite(layout.offset.relative.x) || !std::isfinite(layout.offset.relative.y))
                throw Exceptions::invalid_args(CE_HERE, "TGUI layout offsets must be finite");
            if (layout.scalable) {
                if (!layout.scalable->width && !layout.scalable->height)
                    throw Exceptions::invalid_args(CE_HERE, "TGUI scalable layout requires a width or height fraction");
                validate_axis(layout.scalable->width, layout.scalable->min_width, layout.scalable->max_width);
                validate_axis(layout.scalable->height, layout.scalable->min_height, layout.scalable->max_height);
            }
        }

        tgui::Layout scaled_axis(const tgui::Layout& parent, const float fraction, const float minimum, const float maximum) {
            auto size = tgui::bindMax(minimum, parent * fraction);
            if (std::isfinite(maximum))
                size = tgui::bindMin(maximum, size);
            return size;
        }

        bool clipboard_shortcut(const tgui::Event& event) {
            return event.type == tgui::Event::Type::KeyPressed &&
                   (tgui::keyboard::isKeyPressCopy(event.key) || tgui::keyboard::isKeyPressCut(event.key) ||
                    tgui::keyboard::isKeyPressPaste(event.key));
        }
    }

    struct Session::State {
        struct ManagedSize {
            std::weak_ptr<tgui::Widget> widget;
            bool width = false;
            bool height = false;
        };

        Input::iInputSystem& input;
        const Input::FocusId target;
        const std::thread::id thread = std::this_thread::get_id();
        const Capabilities capabilities;
        Input::CaptureLease events;
        Input::CaptureLease text;
        Input::FocusLease focus;
        std::uint64_t focus_epoch = 0;
        bool drawable = false;
        bool pointer_view = false;
        std::unordered_map<tgui::Widget*, ManagedSize> managed_sizes;
        // Destruction order keeps the backend alive through GUI/font teardown.
        BackendLease backend;
        std::shared_ptr<RenderTarget> render_target = std::make_shared<RenderTarget>();
        Gui gui{render_target};

        State(Input::iInputSystem& source, const Input::FocusId id, const SessionOptions options)
        : input(source),
          target(id),
          capabilities{source.supports_focus(), source.supports(Input::InputMode::Text)},
          events(source.capture(Input::InputMode::Events)),
          backend(options) {}
    };

    Session::Session(Input::iInputSystem& input, const Input::FocusId target, const SessionOptions options) {
        if (target == 0)
            throw Exceptions::invalid_args(CE_HERE, "TGUI requires a nonzero focus target");
        state_ = std::make_unique<State>(input, target, options);
    }

    Session::~Session() = default;

    Session::State& Session::owner() const {
        if (!state_ || state_->thread != std::this_thread::get_id())
            throw Exceptions::failed_operation(CE_HERE, "TGUI calls require the live simulation/UI owner");
        return *state_;
    }

    tgui::BackendGui& Session::gui() {
        return owner().gui;
    }

    Capabilities Session::capabilities() const {
        return owner().capabilities;
    }

    void Session::set_view(const ViewPort<int> logical, const FramebufferSize framebuffer) {
        auto& state = owner();
        if (logical.width < 0 || logical.height < 0 || framebuffer.width < 0 || framebuffer.height < 0)
            throw Exceptions::invalid_args(CE_HERE, "TGUI window dimensions must be nonnegative");
        state.pointer_view = logical.width > 0 && logical.height > 0;
        state.drawable = state.pointer_view && framebuffer.width > 0 && framebuffer.height > 0;
        state.render_target->set_pixel_scale(
            state.drawable ? tgui::Vector2f{static_cast<float>(framebuffer.width) / logical.width,
                                            static_cast<float>(framebuffer.height) / logical.height}
                           : tgui::Vector2f{1, 1}
        );
        state.gui.set_logical_size(logical);
    }

    void Session::set_layout(tgui::Widget::Ptr widget, const WidgetLayout& layout) {
        auto& state = owner();
        validate_layout(layout);
        if (!widget || !widget->getParent())
            throw Exceptions::invalid_args(CE_HERE, "TGUI layout requires a widget added to this session's GUI");
        if (widget->getAutoLayout() != tgui::AutoLayout::Manual)
            throw Exceptions::invalid_args(CE_HERE, "TGUI typed placement requires manual widget layout");
        auto* parent = widget->getParent();
        auto* ancestor = parent;
        const auto root = state.gui.getContainer();
        while (ancestor && ancestor != root.get())
            ancestor = ancestor->getParent();
        if (!ancestor)
            throw Exceptions::invalid_args(CE_HERE, "TGUI layout widget belongs to another hierarchy");

        const auto container = std::static_pointer_cast<tgui::Container>(parent->shared_from_this());
        const auto parent_width = tgui::bindInnerWidth(container);
        const auto parent_height = tgui::bindInnerHeight(container);
        const auto origin = layout.origin.value_or(layout.anchor);
        const tgui::Layout2d position{parent_width * (layout.anchor.x + layout.offset.relative.x) + layout.offset.fixed.x,
                                      parent_height * (layout.anchor.y + layout.offset.relative.y) + layout.offset.fixed.y};

        std::erase_if(state.managed_sizes, [](const auto& entry) { return entry.second.widget.expired(); });
        const auto previous = state.managed_sizes.find(widget.get());
        const bool had_width = previous != state.managed_sizes.end() && previous->second.width;
        const bool had_height = previous != state.managed_sizes.end() && previous->second.height;
        const bool width = layout.scalable && layout.scalable->width.has_value();
        const bool height = layout.scalable && layout.scalable->height.has_value();
        auto size = widget->getSizeLayout();
        if (width)
            size.x = scaled_axis(parent_width, *layout.scalable->width, layout.scalable->min_width, layout.scalable->max_width);
        else if (had_width)
            size.x = widget->getSize().x;
        if (height)
            size.y = scaled_axis(parent_height, *layout.scalable->height, layout.scalable->min_height, layout.scalable->max_height);
        else if (had_height)
            size.y = widget->getSize().y;
        if (!std::isfinite(position.x.getValue()) || !std::isfinite(position.y.getValue()) || !std::isfinite(size.x.getValue()) ||
            !std::isfinite(size.y.getValue()))
            throw Exceptions::invalid_args(CE_HERE, "TGUI layout produces nonfinite coordinates or dimensions");

        if (width || height)
            state.managed_sizes.insert_or_assign(widget.get(), State::ManagedSize{widget, width, height});
        else
            state.managed_sizes.erase(widget.get());
        if (width || height || had_width || had_height)
            widget->setSize(size);
        widget->setOrigin(origin);
        widget->setPosition(position);
    }

    void Session::update_time(const double seconds) {
        auto& state = owner();
        const auto maximum = std::chrono::duration<double>(std::chrono::nanoseconds::max()).count();
        if (!std::isfinite(seconds) || seconds < 0 || seconds >= maximum)
            throw Exceptions::invalid_args(CE_HERE, "TGUI timing requires a finite nonnegative representable duration");
        state.gui.updateTime(std::chrono::duration<double>(seconds));
    }

    void Session::request_keyboard_focus(const Input::KeyboardRouting routing) {
        auto& state = owner();
        if (!state.capabilities.keyboard_focus || !state.capabilities.committed_text)
            throw Exceptions::failed_operation(CE_HERE, "TGUI keyboard focus requires routed committed text support");
        auto text = state.input.capture(Input::InputMode::Text);
        auto focus = state.input.routing().focus(state.target, routing);
        state.text = std::move(text);
        state.focus = std::move(focus);
        state.focus_epoch = state.focus.epoch();
    }

    void Session::release_keyboard_focus() {
        auto& state = owner();
        state.focus.reset();
        state.text.reset();
        state.gui.unfocusAllWidgets();
    }

    bool Session::owns_keyboard_focus() const {
        return owner().focus.owns_focus();
    }

    void Session::handle_input(const std::span<const Input::InputRecord> records, const bool pointer_selected) {
        handle_input(records, pointer_selected, {});
    }

    void Session::handle_input(
        const std::span<const Input::InputRecord> records,
        bool pointer_selected,
        const std::function<bool(const Input::InputRecord&)>& before_record
    ) {
        auto& state = owner();
        const auto focus_epoch = state.focus_epoch;
        for (const auto& record : records) {
            if (before_record)
                pointer_selected = before_record(record);
            if (const auto* button = std::get_if<Input::ButtonEvent>(&record.data);
                button && (record.device_kind == Input::DeviceKind::Keyboard || record.device_kind == Input::DeviceKind::Mouse))
                state.gui.set_modifiers(button->modifiers);
            const bool keyboard = record.is_text() || record.device_kind == Input::DeviceKind::Keyboard;
            if (keyboard) {
                if (record.target != state.target || focus_epoch == 0 || record.focus_epoch != focus_epoch)
                    continue;
            } else if (record.device_kind != Input::DeviceKind::Mouse || !pointer_selected || !state.pointer_view)
                continue;
            const auto event = translate_event(record);
            // Unavailable clipboard shortcuts must not cut/delete a selection.
            if (event && !clipboard_shortcut(*event))
                state.gui.handleEvent(*event);
        }
        // Already collected records retain their poll-latched owner. Releasing an
        // old lease cannot clear a newer owner; stop editing after draining them.
        if (state.focus.epoch() != 0 && !state.focus.owns_focus())
            release_keyboard_focus();
    }

    RecordedScene Session::record() {
        auto& state = owner();
        try {
            if (state.drawable)
                state.gui.draw();
            else
                state.render_target->begin_recording();
            return state.render_target->finish_recording();
        } catch (...) {
            state.render_target->discard_recording();
            throw;
        }
    }

    void Session::close_after_quiescence() {
        state_.reset();
    }
}
