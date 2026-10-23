#include "gate.h"

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace GateConsole {
    D::OperationKeys operation_keys() {
        return {{{"introduced"}}, {{"introduce"}}};
    }

    D::DefinitionHandle gate_definition() {
        const D::Draft draft{
            .id = {"gate-encounter"},
            .revision = 1,
            .entry = {"approach"},
            .nodes = {
                {{"approach"},
                 "A guard waits.",
                 D::NodeKind::Prompt,
                 {{{"introduce"}, "Introduce yourself", {"gate"}, {}, D::EffectKey{"introduce"}},
                  {{"silent"}, "Stay silent", {"gate"}, {}, {}}}},
                {{"gate"},
                 "The gate is closed.",
                 D::NodeKind::Prompt,
                 {{{"pass"}, "Ask to enter", {"admitted"}, D::ConditionKey{"introduced"}, {}}, {{"leave"}, "Leave", {"departed"}, {}, {}}}},
                {{"admitted"}, "You enter", D::NodeKind::Terminal, {}},
                {{"departed"}, "You depart", D::NodeKind::Terminal, {}}
            }
        };
        return D::compile(draft, operation_keys()).value();
    }

    std::string_view describe(D::SessionError error) noexcept {
        switch (error) {
            case D::SessionError::InvalidDefinition:
                return "invalid definition";
            case D::SessionError::EmptySessionId:
                return "empty session ID";
            case D::SessionError::SnapshotSchemaMismatch:
                return "unsupported snapshot schema";
            case D::SessionError::DefinitionMismatch:
                return "definition mismatch";
            case D::SessionError::RevisionMismatch:
                return "definition revision mismatch";
            case D::SessionError::UnknownNode:
                return "unknown node";
            case D::SessionError::MovedFrom:
                return "session has been moved";
            case D::SessionError::Busy:
                return "session busy";
            case D::SessionError::Faulted:
                return "session faulted; application state may be partial";
            case D::SessionError::Finished:
                return "encounter finished";
            case D::SessionError::WrongSession:
                return "foreign session selection";
            case D::SessionError::WrongIncarnation:
                return "stale session incarnation";
            case D::SessionError::StaleSelection:
                return "stale selection";
            case D::SessionError::UnknownChoice:
                return "unknown choice";
            case D::SessionError::Unavailable:
                return "choice unavailable";
            case D::SessionError::StepExhausted:
                return "step exhausted";
            case D::SessionError::MissingConditionHandler:
                return "missing condition handler";
            case D::SessionError::MissingEffectHandler:
                return "missing effect handler";
        }
        return "unknown session error";
    }

    Encounter::Encounter(std::size_t* effect_calls)
    : Encounter(gate_definition(), effect_calls) {}

    Encounter::Encounter(D::DefinitionHandle definition, std::size_t* effect_calls)
    : definition_(std::move(definition)), session_(D::Session::start(definition_, {"gate-0001"}).value()), effect_calls_(effect_calls) {}

    std::expected<D::View, D::SessionError> Encounter::view() {
        return session_.view([this](const D::ConditionKey& key) {
            if (key.value != "introduced")
                throw std::logic_error("Unknown gate condition");
            return introduced_;
        });
    }

    std::expected<void, D::SessionError> Encounter::choose(const D::Selection& selection) {
        return session_.choose(
            selection,
            [this](const D::ConditionKey& key) {
                if (key.value != "introduced")
                    throw std::logic_error("Unknown gate condition");
                return introduced_;
            },
            [this](const D::EffectKey& key) {
                if (key.value != "introduce")
                    throw std::logic_error("Unknown gate effect");
                introduced_ = true;
                if (effect_calls_)
                    ++*effect_calls_;
            }
        );
    }

    std::expected<PairedState, D::SessionError> Encounter::capture() {
        auto snapshot = session_.snapshot();
        if (!snapshot)
            return std::unexpected(snapshot.error());
        return PairedState{std::move(*snapshot), introduced_};
    }

    std::expected<void, D::SessionError> Encounter::restore(const PairedState& state) {
        static_assert(std::is_nothrow_move_assignable_v<D::Session>);
        static_assert(std::is_nothrow_copy_constructible_v<std::expected<void, D::SessionError>>);
        auto loaded = D::Session::resume(definition_, state.dialogue);
        if (!loaded)
            return std::unexpected(loaded.error());
        const std::expected<void, D::SessionError> success;
        // Both values are idle and all validation/allocation is complete.
        session_ = std::move(*loaded);
        introduced_ = state.introduced;
        return success;
    }
}
