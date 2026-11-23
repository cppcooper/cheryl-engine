#pragma once

#include <cheryl/tools/console/model.h>
#include <cheryl/tools/console/runtime/runner.h>
#include <cheryl/tools/console/setting.h>

#include <functional>
#include <stdexcept>

namespace Independent {
    namespace Core = CE::Tools::Console;
    namespace Script = Core::Scenario;
    namespace Runtime = Core::Runtime;

    struct Simulation {
        std::uint64_t step_limit = 4, steps = 0, writes = 0;
        bool set_limit(std::uint64_t value) noexcept {
            if (value > 16) return false;
            step_limit = value; ++writes; return true;
        }
        void advance() noexcept { steps += step_limit; }
    };

    class CounterWork final : public Core::Work {
        std::shared_ptr<Simulation> owner_;
        bool advance_;
    public:
        CounterWork(std::shared_ptr<Simulation> owner, bool advance)
            : owner_(std::move(owner)), advance_(advance) { result.value = Core::Value::unsigned_integer(0); }
        void step() noexcept override {
            if (Core::terminal(result.stage)) return;
            if (advance_) owner_->advance();
            result.value.data = owner_->steps;
            result.stage = Core::Stage::Succeeded;
        }
        void close() noexcept override {
            if (!Core::terminal(result.stage)) {
                result.stage = Core::Stage::Cancelled; result.failure = {Core::Code::Cancelled};
            }
        }
    };

    class Host final : public Runtime::Fixture, public Core::Presentation::Connection {
        Core::Authority authority_;
        Core::Context gameplay_ = authority_.issue(3), diagnostic_ = authority_.issue(1);
        Core::Registry registry_{[this](const Core::Context& context) {
            return context.same_scope(gameplay_) || context.same_scope(diagnostic_);
        }};
        std::shared_ptr<Simulation> simulation_ = std::make_shared<Simulation>();
        Core::UnsignedSetting setting_{registry_, "simulation.step_limit", "Substeps per owner tick; zero pauses simulation",
            4, 16,
            {[owner = simulation_] { return owner->step_limit; },
             [owner = simulation_](std::uint64_t value) { return owner->set_limit(value); }}, 1, 2};
        Core::Registration advance_, read_;
        Core::Session gameplay_session_{gameplay_}, diagnostic_session_{diagnostic_};
    public:
        Host() {
            Core::Descriptor descriptor;
            descriptor.name = "simulation.advance";
            descriptor.help = "Advances one tick using the actual current step limit";
            descriptor.domain = "simulation"; descriptor.kind = Core::Kind::Action; descriptor.rights = 2;
            descriptor.result.type = Core::Type::Unsigned;
            auto added = registry_.add<>(descriptor, [owner = simulation_](const Core::Context&) {
                return std::make_shared<CounterWork>(owner, true);
            });
            if (!added) throw std::runtime_error("Cannot register independent advance");
            advance_ = std::move(*added);
            descriptor.name = "simulation.steps"; descriptor.help = "Reads completed substeps on the host owner";
            descriptor.kind = Core::Kind::Query; descriptor.rights = 1;
            added = registry_.add<>(descriptor, [owner = simulation_](const Core::Context&) {
                return std::make_shared<CounterWork>(owner, false);
            });
            if (!added) throw std::runtime_error("Cannot register independent query");
            read_ = std::move(*added);
        }
        ~Host() override { close(); }
        Core::Registry& registry() noexcept override { return registry_; }
        Script::Channels channels() noexcept override {
            return {{gameplay_, gameplay_session_}, Script::Channel{diagnostic_, diagnostic_session_}};
        }
        Core::Context context() const noexcept override { return gameplay_; }
        Core::Session& session() noexcept override { return gameplay_session_; }
        std::string scope() const override { return "Independent simulation <owner-thread>"; }
        void close() noexcept override { gameplay_session_.close(); diagnostic_session_.close(); }
        const Simulation& simulation() const noexcept { return *simulation_; }
    };

    inline std::unique_ptr<Runtime::Fixture> fixture(std::string_view name, std::string_view asset) {
        if (name != "independent-v1" || !asset.empty()) throw std::runtime_error("Unknown independent fixture");
        return std::make_unique<Host>();
    }
}
