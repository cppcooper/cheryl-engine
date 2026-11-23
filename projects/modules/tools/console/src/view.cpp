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
        std::uint64_t activity = 0;
        int scroll_after_layout = 0;
        bool visible = false, changing = false;
        bool input_selected = true;
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
                element("console-scope").SetInnerRML(escaped(model.scope()));
                field->AddEventListener("keydown", this, true);
                field->AddEventListener("change", this);
                for (const auto* id : buttons)
                    element(id).AddEventListener("click", this);
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
                field->RemoveEventListener("keydown", this, true);
                field->RemoveEventListener("change", this);
            }
            for (const auto* id : buttons)
                if (auto* button = document->GetElementById(id))
                    button->RemoveEventListener("click", this);
            for (const auto& prepared : options.prepared_commands)
                if (auto* button = document->GetElementById(prepared.first))
                    button->RemoveEventListener("click", this);
            document->Close();
            document = nullptr;
        }

        void suggestions() {
            const auto value = field->GetValue();
            const auto suggestions = model.suggestions(value);
            std::string text;
            for (const auto& suggestion : suggestions) {
                if (!text.empty())
                    text += " | ";
                text += display_text(suggestion);
            }
            candidates->SetInnerRML(escaped(text));
        }

        void set_input(std::string value) {
            changing = true;
            field->SetValue(value);
            changing = false;
            suggestions();
            static_cast<void>(field->Focus(true));
        }

        void hide() {
            if (!visible)
                return;
            ++activity;
            visible = false;
            document->Hide();
            session.release_keyboard_focus();
        }

        void submit() {
            if (model.submit(field->GetValue()))
                set_input({});
            scroll_after_layout = 2;
        }

        void refresh() {
            const auto text = model.text();
            if (text != rendered) {
                output->SetInnerRML(escaped(text));
                rendered = text;
            }
            if (scroll_after_layout && --scroll_after_layout == 0)
                output->SetScrollTop(output->GetScrollHeight());
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
            } else if (event.GetType() == "keydown") {
                const auto key = event.GetParameter<int>("key_identifier", 0);
                if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER)
                    submit();
                else if (key == Rml::Input::KI_UP)
                    set_input(model.previous(field->GetValue()));
                else if (key == Rml::Input::KI_DOWN)
                    set_input(model.next());
                else if (key == Rml::Input::KI_TAB)
                    set_input(model.complete(field->GetValue()));
                else
                    return;
                event.StopPropagation();
            } else if (event.GetType() == "click") {
                const auto id = event.GetCurrentElement()->GetId();
                ++activity;
                if (id == "console-submit")
                    submit();
                else if (id == "console-previous")
                    set_input(model.previous(field->GetValue()));
                else if (id == "console-next")
                    set_input(model.next());
                else if (id == "console-complete")
                    set_input(model.complete(field->GetValue()));
                else if (id == "console-help") {
                    const auto parsed = Core::parse(field->GetValue(), true);
                    model.help(parsed && !parsed->tokens.empty() ? parsed->tokens.front().text : std::string{});
                    scroll_after_layout = 2;
                } else if (id == "console-latest")
                    scroll_after_layout = 2;
                else if (id == "console-close")
                    hide();
                else {
                    for (const auto& prepared : options.prepared_commands)
                        if (id == prepared.first) set_input(prepared.second);
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
