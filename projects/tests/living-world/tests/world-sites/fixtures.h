#pragma once

#include "sites.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace CE::LivingWorld::Sites::Fixture {
    inline void check(bool condition, std::string_view message) {
        if (!condition) { throw std::runtime_error(std::string(message)); }
    }
    template <class Value, class FailureValue>
    Value take(std::expected<Value, FailureValue> value) {
        check(value.has_value(), "fixture setup or required operation fails");
        return std::move(*value);
    }
    inline Text text(std::string_view value) { return take(Text::from(value)); }
    struct Setup {
        WaterFacts water;
        Batch batch;
        Preserved preserved;
    };
    [[nodiscard]] Setup generated();
    [[nodiscard]] Setup literal();
    [[nodiscard]] std::array<ReachFact, 3> facts(bool diverted = false);
    [[nodiscard]] std::array<Binding, 3> bindings(const Stamp& stamp);
    [[nodiscard]] Preserved preserved();
    [[nodiscard]] std::unique_ptr<Owner> owner(const Setup& setup);
    [[nodiscard]] Candidate change(const Image& image, Index sequence, bool diverted = true);
    [[nodiscard]] Batch reassess(const WaterFacts& water);
}
