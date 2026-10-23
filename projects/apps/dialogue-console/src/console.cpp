#include "console.h"
#include "files.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <istream>
#include <limits>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>

namespace GateConsole {
    int run_console(Encounter& encounter, std::istream& input, std::ostream& output) {
        std::optional<D::View> presented;
        std::uint64_t offer = 0;
        const auto show = [&]() {
            auto view = encounter.view();
            if (!view) {
                output << "Cannot show: " << describe(view.error()) << '\n';
                return;
            }
            if (offer == std::numeric_limits<std::uint64_t>::max()) {
                output << "Presentation counter exhausted; quit and resume in a new process.\n";
                presented.reset();
                return;
            }
            presented = std::move(*view);
            output << "Offer " << ++offer << '\n' << presented->text << '\n';
            for (const auto& row : presented->choices)
                output << "  " << row.id.value << ": " << row.text << (row.available ? "" : " [unavailable]") << '\n';
            if (presented->kind == D::NodeKind::Terminal)
                output << "(End)\n";
        };
        output << "Commands: show, choose OFFER CHOICE, save PATH, resume PATH, quit\n";
        show();
        std::string line;
        while (output && std::getline(input, line)) {
            std::istringstream command(line);
            std::string verb;
            command >> verb;
            if (verb == "save" || verb == "resume") {
                std::string path;
                std::getline(command >> std::ws, path);
                const auto result = verb == "save" ? save_fresh(encounter, path) : resume_file(encounter, path);
                if (!result) {
                    output << "Rejected: " << result.error() << '\n';
                } else if (verb == "save") {
                    output << "Saved: " << path << '\n';
                } else {
                    output << "Resumed: " << path << '\n';
                    presented.reset();
                    show();
                }
                continue;
            }
            if (verb == "choose") {
                std::string number, choice, extra;
                command >> number >> choice;
                std::uint64_t requested = 0;
                const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), requested);
                if (error != std::errc{} || end != number.data() + number.size() || choice.empty() || command >> extra) {
                    output << "Rejected: use choose OFFER CHOICE.\n";
                    continue;
                }
                if (!presented || requested != offer) {
                    output << "Rejected: stale presentation; use show.\n";
                    continue;
                }
                const auto row = std::ranges::find(presented->choices, D::ChoiceId{choice}, &D::ChoiceView::id);
                if (row == presented->choices.end()) {
                    output << "Rejected: unknown choice.\n";
                    continue;
                }
                const auto result = encounter.choose(row->selection);
                if (!result)
                    output << "Rejected: " << describe(result.error()) << '\n';
                else {
                    presented.reset();
                    show();
                }
                continue;
            }
            std::string extra;
            if (command >> extra || (verb != "quit" && verb != "show")) {
                output << "Rejected: unknown command.\n";
            } else if (verb == "quit") {
                output << "Goodbye.\n";
                return output ? 0 : 1;
            } else {
                show();
            }
        }
        if (input.bad())
            output << "Input failed.\n";
        return !input.bad() && output ? 0 : 1;
    }
}
