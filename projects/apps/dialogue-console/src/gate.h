#pragma once

#include <cheryl/core/game-framework/dialogue.h>

#include <cstddef>
#include <expected>
#include <string_view>

namespace GateConsole {
    namespace D = CE::Dialogue;

    struct PairedState {
        D::Snapshot dialogue;
        bool introduced = false;
        bool operator==(const PairedState&) const = default;
    };

    D::DefinitionHandle gate_definition();
    std::string_view describe(D::SessionError error) noexcept;

    // One serialized owner. The optional counter is borrowed test instrumentation,
    // never gameplay state, saved data or a receipt for an external effect.
    class Encounter {
        D::DefinitionHandle definition_;
        D::Session session_;
        bool introduced_ = false;
        std::size_t* effect_calls_;

    public:
        explicit Encounter(std::size_t* effect_calls = nullptr);
        Encounter(const Encounter&) = delete;
        Encounter& operator=(const Encounter&) = delete;

        std::expected<D::View, D::SessionError> view();
        std::expected<void, D::SessionError> choose(const D::Selection& selection);
        std::expected<PairedState, D::SessionError> capture();
        std::expected<void, D::SessionError> restore(const PairedState& state);
    };
}
