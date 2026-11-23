#pragma once

#include <cheryl/tools/console/scenario.h>
#include <optional>

namespace CE::Tools::Console::Scenario::Detail {
    struct Reference { std::size_t name = 0; std::vector<std::size_t> fields; Schema schema; };
    struct Input { Value literal; std::optional<Reference> reference; };
    enum class Kind { Command, Await, Retry, Expect, Finish };
    struct Statement {
        Kind kind = Kind::Command;
        std::size_t line = 0, output = 0, source = 0, budget = 0;
        Descriptor descriptor;
        std::optional<BoundRequest> bound;
        std::vector<Input> inputs;
        Stage expected = Stage::Succeeded;
        Code expected_error = Code::None;
        Reference reference;
        Value literal;
        std::string spelling;
        Route route = Route::Gameplay;
        std::optional<ResolvedCommand> resolved;
    };
    struct Name { std::string text; Schema schema; std::size_t line = 0; Stage expected; Code error; Route route; };
    struct PlanData {
        Context context;
        std::optional<Context> diagnostic;
        const Registry* registry = nullptr;
        Session* gameplay_session = nullptr;
        Session* diagnostic_session = nullptr;
        bool routed = false;
        mutable bool started = false;
        std::string fixture;
        std::vector<Statement> statements;
        std::vector<Name> names;
        std::size_t assertions = 0;
        const Context& selected(Route route) const noexcept {
            return route == Route::Gameplay ? context : *diagnostic;
        }
    };
    bool fresh_channels(const PlanData&) noexcept;
    Checked<Value> literal(std::string_view, const Schema&, const Context&, std::size_t line);
    Checked<Reference> reference(const PlanData&, std::string_view, std::size_t line);
    const Value* value(const NamedResult&, const Reference&) noexcept;
}
