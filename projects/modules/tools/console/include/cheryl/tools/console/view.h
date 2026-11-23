#pragma once

#include <cheryl/export.h>
#include <cheryl/core/controls/input-record.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <utility>

namespace CE::UI::RmlUi {
    class Session;
}
namespace CE::Tools::Console::Presentation {
    class Model;
    /** Empty document selects the host-neutral embedded document. Custom RML
     * supplies the console-* controls described in the Console guide. Prepared
     * commands map element IDs to command lines; they do not bypass binding.
     */
    struct ViewOptions {
        std::string document;
        std::vector<std::pair<std::string, std::string>> prepared_commands;
    };

    /** Borrows Model and the RmlUi Session on their common owner thread.
     * Both outlive View; destroy View before closing/destroying the UI Session.
     * Construction failure removes installed listeners and closes its document.
     * Input routing and gameplay quarantine remain the application's job.
     */
    class CE_TYPE_API View {
        struct State;
        std::unique_ptr<State> state_;

    public:
        CE_CONSOLE_API View(CE::UI::RmlUi::Session& session, Model& model, ViewOptions options = {});
        CE_CONSOLE_API virtual ~View();
        View(const View&) = delete;
        View& operator=(const View&) = delete;
        CE_CONSOLE_API void open();
        CE_CONSOLE_API void hide();
        CE_CONSOLE_API void update();
        CE_CONSOLE_API void select_input(bool selected) noexcept;
        CE_CONSOLE_API bool visible() const noexcept;
        CE_CONSOLE_API bool inside(const CE::Input::Events::Pointer& position) const;
        CE_CONSOLE_API std::uint64_t activity() const noexcept;
    };
}
