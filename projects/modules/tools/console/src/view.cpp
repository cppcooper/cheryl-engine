#include <cheryl/tools/console/view.h>
#include <cheryl/tools/console/model.h>
#include "console-document.h"

#include <ui/rmlui/session.h>
#include <stdexcept>

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/ScrollTypes.h>

#include <algorithm>
#include <string>
#include <utility>

namespace CE::Tools::Console::Presentation {
    struct View::State final : Rml::EventListener {
        CE::UI::RmlUi::Session& session;
        Model& model;
        ViewOptions options;
        Rml::ElementDocument* document = nullptr;
        Rml::ElementFormControlInput* field = nullptr;
        Rml::Element* output = nullptr;
        Rml::Element* candidates = nullptr;
        std::string rendered;
        std::string completion_source;
        std::vector<std::string> matches;
        int selected_match = -1;
        std::uint64_t activity = 0;
        bool scroll_to_latest = false, scroll_candidate = false, suggestions_dirty = false;
        bool visible = false, changing = false;
        bool input_selected = true;
        bool follow_output = true;
        static constexpr const char* buttons[] = {
            "console-submit", "console-previous", "console-next", "console-complete", "console-help", "console-latest",
            "console-close"
        };

        Rml::Element& element(const char* id) const {
            auto* result = document->GetElementById(id);
            if (!result)
                throw std::runtime_error("Missing console element: " + std::string{id});
            return *result;
        }

        State(CE::UI::RmlUi::Session& selected, Model& controller, ViewOptions configured)
            : session(selected), model(controller), options(std::move(configured)) {
            document = session.context().LoadDocumentFromMemory(Rml::String{options.document.empty() ? std::string{default_document} : options.document});
            if (!document)
                throw std::runtime_error("Cannot load the embedded developer console");
            try {
                field = dynamic_cast<Rml::ElementFormControlInput*>(&element("console-command"));
                if (!field)
                    throw std::runtime_error("Console requires a native text input");
                output = &element("console-output");
                candidates = &element("console-suggestions");
                if (auto* scope = document->GetElementById("console-scope"))
                    scope->SetInnerRML(escaped(model.scope()));
                document->AddEventListener("keydown", this, true);
                document->AddEventListener("mouseup", this, true);
                field->AddEventListener("change", this);
                candidates->AddEventListener("click", this);
                output->AddEventListener("mousescroll", this);
                for (const auto* id : buttons)
                    if (auto* button = document->GetElementById(id))
                        button->AddEventListener("click", this);
                for (const auto& prepared : options.prepared_commands)
                    element(prepared.first.c_str()).AddEventListener("click", this);
                refresh();
            } catch (...) {
                detach();
                throw;
            }
        }

        ~State() { detach(); }
        void detach() {
            if (!document)
                return;
            if (field) {
                field->RemoveEventListener("change", this);
            }
            document->RemoveEventListener("keydown", this, true);
            document->RemoveEventListener("mouseup", this, true);
            if (candidates)
                candidates->RemoveEventListener("click", this);
            if (output)
                output->RemoveEventListener("mousescroll", this);
            for (const auto* id : buttons)
                if (auto* button = document->GetElementById(id))
                    button->RemoveEventListener("click", this);
            for (const auto& prepared : options.prepared_commands)
                if (auto* button = document->GetElementById(prepared.first))
                    button->RemoveEventListener("click", this);
            document->Close();
            document = nullptr;
        }

        void render_suggestions() {
            std::string text;
            for (std::size_t index = 0; index < matches.size(); ++index) {
                text += "<div class=\"console-suggestion";
                if (static_cast<int>(index) == selected_match)
                    text += " selected";
                text += "\" candidate-index=\"" + std::to_string(index) + "\">";
                text += escaped(display_text(matches[index])) + "</div>";
            }
            candidates->SetInnerRML(text);
            candidates->SetProperty("display", matches.empty() ? "none" : "block");
            scroll_candidate = selected_match >= 0;
            suggestions_dirty = true;
            place_suggestions();
        }

        void place_suggestions() {
            auto* row = candidates->GetParentNode();
            if (matches.empty() || !row || row->GetId() != "console-command-row")
                return;
            const auto top = row->GetAbsoluteOffset().y;
            const auto height = row->GetOffsetHeight();
            const auto below = std::max(0.0f, session.context().GetDimensions().y - top - height - 8);
            const auto above = std::max(0.0f, top - 8);
            const bool upwards = below < std::min(220.0f, 26.0f * matches.size() + 2) && above > below;
            candidates->SetProperty("max-height", std::to_string(static_cast<int>(std::min(220.0f, upwards ? above : below))) + "px");
            candidates->SetProperty("top", upwards ? "auto" : std::to_string(static_cast<int>(height - 1)) + "px");
            candidates->SetProperty("bottom", upwards ? std::to_string(static_cast<int>(height - 1)) + "px" : "auto");
        }

        void dismiss_suggestions() {
            matches.clear();
            selected_match = -1;
            completion_source.clear();
            render_suggestions();
        }

        void suggestions() {
            completion_source = field->GetValue();
            selected_match = -1;
            matches = completion_source.empty() ? std::vector<std::string>{} : model.suggestions(completion_source);
            render_suggestions();
        }

        void set_input(std::string value, bool show_suggestions = false) {
            changing = true;
            field->SetValue(value);
            changing = false;
            const auto characters = static_cast<int>(std::count_if(value.begin(), value.end(), [](unsigned char byte) {
                return (byte & 0xc0) != 0x80;
            }));
            field->SetSelectionRange(characters, characters);
            if (show_suggestions)
                suggestions();
            static_cast<void>(field->Focus(true));
        }

        void cycle(int direction) {
            if (matches.empty())
                suggestions();
            if (matches.empty())
                return;
            const auto count = static_cast<int>(matches.size());
            selected_match = selected_match < 0 ? (direction > 0 ? 0 : count - 1)
                : (selected_match + direction + count) % count;
            set_input(model.complete(completion_source, matches[selected_match]));
            render_suggestions();
        }

        void hide() {
            if (!visible)
                return;
            ++activity;
            visible = false;
            dismiss_suggestions();
            document->Hide();
            session.release_keyboard_focus();
        }

        void submit() {
            static_cast<void>(model.submit(field->GetValue()));
            set_input({});
            dismiss_suggestions();
            scroll_to_latest = true;
            follow_output = true;
        }

        static std::string output_markup(std::string_view text) {
            std::string markup;
            while (!text.empty()) {
                const auto newline = text.find('\n');
                const auto line = text.substr(0, newline);
                if (line.starts_with("Error:") || line == "[failed]" || line == "[refused]" || line == "[cancelled]")
                    markup += "<span class=\"console-error\">" + escaped(line) + "</span>";
                else if (line.starts_with("] "))
                    markup += "<span class=\"console-command-line\">" + escaped(line) + "</span>";
                else
                    markup += escaped(line);
                if (newline == std::string_view::npos)
                    break;
                markup += '\n';
                text.remove_prefix(newline + 1);
            }
            return markup;
        }

        void refresh() {
            if (!scroll_to_latest)
                follow_output = output->GetScrollTop() + output->GetClientHeight() >= output->GetScrollHeight() - 2;
            const auto text = model.text();
            const bool changed = text != rendered;
            if (changed) {
                output->SetInnerRML(output_markup(text));
                rendered = text;
            }
            if (changed || scroll_to_latest || suggestions_dirty)
                document->UpdateDocument();
            place_suggestions();
            if (suggestions_dirty) {
                document->UpdateDocument();
                suggestions_dirty = false;
            }
            if (scroll_candidate && selected_match >= 0)
                if (auto* selected = candidates->GetChild(selected_match))
                    selected->ScrollIntoView({Rml::ScrollAlignment::Nearest, Rml::ScrollAlignment::Nearest,
                        Rml::ScrollBehavior::Instant, Rml::ScrollParentage::Closest});
            scroll_candidate = false;
            if (scroll_to_latest || (changed && follow_output))
                output->SetScrollTop(output->GetScrollHeight());
            scroll_to_latest = false;
        }

        void ProcessEvent(Rml::Event& event) override {
            if (!input_selected || !visible || !session.owns_keyboard_focus())
                return;
            if (event.GetType() == "change" && !changing) {
                auto value = field->GetValue();
                if (value.size() > Model::command_bytes) {
                    static_cast<void>(model.suggestions(value));
                    auto size = Model::command_bytes;
                    while (size && (static_cast<unsigned char>(value[size]) & 0xc0) == 0x80)
                        --size;
                    value.resize(size);
                    changing = true;
                    field->SetValue(value);
                    changing = false;
                }
                suggestions();
            } else if (event.GetType() == "mousescroll") {
                scroll_to_latest = false;
                return;
            } else if (event.GetType() == "mouseup") {
                static_cast<void>(field->Focus(true));
                return;
            } else if (event.GetType() == "keydown") {
                const auto key = event.GetParameter<int>("key_identifier", 0);
                if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER)
                    submit();
                else if (key == Rml::Input::KI_UP || key == Rml::Input::KI_DOWN) {
                    if (!matches.empty())
                        cycle(key == Rml::Input::KI_DOWN ? 1 : -1);
                    else {
                        set_input(key == Rml::Input::KI_UP ? model.previous(field->GetValue()) : model.next());
                        dismiss_suggestions();
                    }
                } else if (key == Rml::Input::KI_TAB)
                    cycle(event.GetParameter<int>("shift_key", 0) ? -1 : 1);
                else if (key == Rml::Input::KI_PRIOR || key == Rml::Input::KI_NEXT) {
                    scroll_to_latest = false;
                    output->SetScrollTop(output->GetScrollTop() + (key == Rml::Input::KI_PRIOR ? -1 : 1) *
                        std::max(20.0f, output->GetClientHeight() - 20.0f));
                } else if (key == Rml::Input::KI_END && event.GetParameter<int>("ctrl_key", 0))
                    scroll_to_latest = true;
                else
                    return;
                event.StopPropagation();
            } else if (event.GetType() == "click") {
                const auto id = event.GetCurrentElement()->GetId();
                ++activity;
                if (event.GetCurrentElement() == candidates) {
                    auto* target = event.GetTargetElement();
                    while (target && target != candidates && !target->HasAttribute("candidate-index"))
                        target = target->GetParentNode();
                    const auto index = target ? target->GetAttribute<int>("candidate-index", -1) : -1;
                    if (index >= 0 && index < static_cast<int>(matches.size())) {
                        set_input(model.complete(completion_source, matches[index]));
                        dismiss_suggestions();
                    }
                } else if (id == "console-submit")
                    submit();
                else if (id == "console-previous") {
                    set_input(model.previous(field->GetValue()));
                    dismiss_suggestions();
                } else if (id == "console-next") {
                    set_input(model.next());
                    dismiss_suggestions();
                } else if (id == "console-complete")
                    cycle(1);
                else if (id == "console-help") {
                    const auto parsed = Core::parse(field->GetValue(), true);
                    model.help(parsed && !parsed->tokens.empty() ? parsed->tokens.front().text : std::string{});
                    scroll_to_latest = true;
                } else if (id == "console-latest")
                    scroll_to_latest = true;
                else if (id == "console-close")
                    hide();
                else {
                    for (const auto& prepared : options.prepared_commands)
                        if (id == prepared.first) set_input(prepared.second, true);
                }
                if (visible)
                    static_cast<void>(field->Focus(true));
                event.StopPropagation();
            }
            refresh();
        }
    };

    View::View(CE::UI::RmlUi::Session& session, Model& model, ViewOptions options)
        : state_(std::make_unique<State>(session, model, std::move(options))) {}
    View::~View() = default;
    void View::select_input(bool selected) noexcept { state_->input_selected = selected; }
    void View::open() {
        auto& state = *state_;
        if (state.visible)
            return;
        ++state.activity;
        state.document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        state.visible = true;
        state.session.release_keyboard_focus();
        state.session.request_keyboard_focus();
        if (!state.field->Focus(true)) {
            state.hide();
            throw std::runtime_error("Cannot focus the developer console input");
        }
        state.suggestions();
    }
    void View::hide() { state_->hide(); }
    void View::update() { state_->refresh(); }
    bool View::visible() const noexcept { return state_->visible; }
    std::uint64_t View::activity() const noexcept { return state_->activity; }
    bool View::inside(const CE::Input::Events::Pointer& position) const {
        if (!state_->visible)
            return false;
        auto* hit = state_->session.context().GetElementAtPoint({static_cast<float>(position.x), static_cast<float>(position.y)});
        for (; hit; hit = hit->GetParentNode())
            if (hit == state_->document)
                return true;
        return false;
    }
}
