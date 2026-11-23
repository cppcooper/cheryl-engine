#include <cheryl/tools/console/runtime/command.h>

#include <algorithm>
#include <stdexcept>

namespace CE::Tools::Console::Runtime {
    namespace Detail {
        class ScenarioWork;
        struct CommandState {
            Config config;
            bool active = true;
            std::vector<std::weak_ptr<ScenarioWork>> jobs;
        };
        class ScenarioWork final : public Core::Work {
            std::shared_ptr<CommandState> owner_;
            Config config_;
            std::unique_ptr<Job> job_;
            bool closed_ = false;
            void snapshot() {
                const auto outcome = job_ ? job_->disposition() : Script::Disposition::Cancelled;
                result.value = Core::Value::record({Core::Value::text(std::string(label(outcome))),
                    Core::Value::text(config_.run_id), Core::Value::boolean(job_ && job_->durable())});
                result.stage = outcome == Script::Disposition::Running ? Core::Stage::Pending : Core::Stage::Succeeded;
            }
        public:
            ScenarioWork(std::shared_ptr<CommandState> owner, std::string script, std::string run)
                : owner_(std::move(owner)), config_(owner_->config) {
                config_.script = std::move(script); config_.run_id = std::move(run);
            }
            void step() noexcept override {
                if (closed_ || Core::terminal(result.stage)) return;
                try {
                    if (!owner_->active) { close(); return; }
                    if (!job_) job_ = std::make_unique<Job>(config_);
                    job_->step(); snapshot();
                } catch (...) { close(); result.stage = Core::Stage::Failed; result.failure.code = Core::Code::Allocation; }
            }
            void close() noexcept override {
                if (closed_ || Core::terminal(result.stage)) return;
                closed_ = true;
                if (job_) job_->close();
                try { snapshot(); }
                catch (...) { result.stage = Core::Stage::Failed; result.failure.code = Core::Code::Allocation; }
            }
        };
    }
    Commands::Commands(Core::Registry& registry, Config config, Core::Rights run_rights) : state_(std::make_shared<Detail::CommandState>()) {
        if (!config.io) throw std::runtime_error("test.run requires the host-owned scenario I/O service");
        if (!config.factory) throw std::runtime_error("test.run requires an isolated host factory");
        state_->config = std::move(config);
        Core::Schema text; text.type = Core::Type::Text;
        Core::Schema boolean; boolean.type = Core::Type::Boolean;
        Core::Schema output; output.type = Core::Type::Record;
        output.fields = {"disposition", "run", "durable"}; output.children = {text, text, boolean};
        Core::Descriptor descriptor;
        descriptor.name = "test.run"; descriptor.help = "Runs an isolated bounded scenario; returns a job disposition";
        descriptor.domain = "scenario"; descriptor.kind = Core::Kind::Action;
        descriptor.timing = Core::Timing::OwnerProgress; descriptor.rights = run_rights;
        descriptor.arguments = {{"script", text}, {"run", text}}; descriptor.result = output;
        auto installed = registry.add<std::string, std::string>(std::move(descriptor),
            [state = state_](const Core::Context&, std::string script, std::string run) -> std::shared_ptr<Core::Work> {
                if (!state->active) throw std::runtime_error("Scenario registration is closed");
                std::erase_if(state->jobs, [](const auto& job) { return job.expired(); });
                if (state->jobs.size() >= Core::max_requests) throw std::runtime_error("Scenario job capacity exhausted");
                auto job = std::make_shared<Detail::ScenarioWork>(state, std::move(script), std::move(run));
                state->jobs.push_back(job); return job;
            });
        if (!installed) throw std::runtime_error("Cannot register test.run: " + std::to_string(static_cast<int>(installed.error().code)));
        registration_ = std::move(*installed);
    }
    Commands::~Commands() { close(); }
    void Commands::close() noexcept {
        registration_.reset();
        if (!state_->active) return;
        state_->active = false;
        for (auto& weak : state_->jobs) if (auto job = weak.lock()) job->close();
    }
}
