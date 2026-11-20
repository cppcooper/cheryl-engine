#pragma once

#include "habitat.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace CE::LivingWorld::Habitat::Fixture {
    inline void check(bool condition, std::string_view message) {
        if (!condition) { throw std::runtime_error(std::string(message)); }
    }
    template <class Value, class FailureValue> Value take(std::expected<Value, FailureValue> value) {
        check(value.has_value(), "valid setup or required operation fails");
        return std::move(*value);
    }
    template <class Value> void refused(const Result<Value>& value, Error reason) {
        check(!value && value.error().reason == reason, "expected habitat refusal");
    }
    inline Sites::Text text(std::string_view value) { return take(Sites::Text::from(value)); }
    inline Seed seed() { return {text("habitat-fixture"), 42, 1}; }
    inline Policy policy() { return {1, 1, {Unit::resident_slots, 4}, {Unit::activity_slots, 2}, text("host-garden-opportunity")}; }
    struct Setup {
        std::shared_ptr<Sites::Owner> owner;
        std::shared_ptr<const Eligibility> evidence;
        Sites::Preserved preserved;
    };
    [[nodiscard]] Sites::Preserved preserved();
    [[nodiscard]] Setup literal();
    [[nodiscard]] Setup generated();
    [[nodiscard]] std::unique_ptr<Advisor> advisor(const Setup& setup);
    [[nodiscard]] std::shared_ptr<const Eligibility> literal_evidence(const Setup& setup, Index revision,
        ProcGen::EligibilityCoverage coverage = ProcGen::EligibilityCoverage::complete);
    [[nodiscard]] Sites::Candidate water_change(const Setup& setup);
}
