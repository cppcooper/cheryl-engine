#include <cheryl/tools/console/runtime/diagnostics.h>
#include <stdexcept>

namespace CE::Tools::Console::Runtime {
    namespace {
        class Observation final : public Work {
            std::function<Value()> sample_;
        public:
            explicit Observation(std::function<Value()> sample) : sample_(std::move(sample)) {}
            void step() noexcept override {
                if (terminal(result.stage)) return;
                try { result.value = sample_(); result.stage = Stage::Succeeded; }
                catch (...) { result.stage = Stage::Failed; result.failure = {Code::Owner}; }
            }
            void close() noexcept override {
                if (!terminal(result.stage)) { result.stage = Stage::Cancelled; result.failure = {Code::Cancelled}; }
            }
        };
        Registration install(Registry& registry, std::string name, std::string help,
            std::vector<std::string> fields, std::function<Value()> sample, Rights rights) {
            Descriptor descriptor;
            descriptor.name = std::move(name); descriptor.help = std::move(help);
            descriptor.domain = "engine-observation"; descriptor.rights = rights;
            descriptor.result.type = Type::Record; descriptor.result.fields = std::move(fields);
            Schema count; count.type = Type::Unsigned;
            descriptor.result.children.assign(descriptor.result.fields.size(), count);
            auto added = registry.add<>(std::move(descriptor), [sample = std::move(sample)](const Context&) {
                return std::make_shared<Observation>(sample);
            });
            if (!added) throw std::runtime_error("Cannot register engine observation");
            return std::move(*added);
        }
    }
    EngineObservations::EngineObservations(Registry& registry, std::string prefix, CE::Workers::Group group,
        std::shared_ptr<CE::Logging::LogHandle> logger, Rights rights) {
        group_ = install(registry, prefix + ".group", "Logical worker group; synchronized sample at owner step, not a barrier",
            {"domain", "pool", "accepted", "completed", "pending", "running", "policy_failures", "callback_failures"},
            [group] {
                const auto status = group.status();
                return Value::record({Value::unsigned_integer(status.domain), Value::unsigned_integer(status.pool),
                    Value::unsigned_integer(status.accepted), Value::unsigned_integer(status.completed),
                    Value::unsigned_integer(status.pending), Value::unsigned_integer(status.running),
                    Value::unsigned_integer(status.policy_failures), Value::unsigned_integer(status.callback_failures)});
            }, rights);
        if (!logger) return;
        backend_ = install(registry, prefix + ".logging_backend", "Retained logger; independently sampled cumulative backend failures, not output durability",
            {"failed_operations", "callback_failures", "suppressed_operations", "recursive_submissions", "rejected_reentry"},
            [logger] {
                const auto status = logger->backend_stats();
                return Value::record({Value::unsigned_integer(status.failed_operations), Value::unsigned_integer(status.callback_failures),
                    Value::unsigned_integer(status.suppressed_operations), Value::unsigned_integer(status.recursive_submissions),
                    Value::unsigned_integer(status.rejected_reentry)});
            }, rights);
        queue_ = install(registry, prefix + ".logging_queue", "Shared logging pool across ALL categories; independent counts, not a logger-local queue",
            {"queued_items", "discarded_items"}, [logger] {
                const auto status = logger->shared_queue_stats();
                return Value::record({Value::unsigned_integer(status.queued_items), Value::unsigned_integer(status.discarded_items)});
            }, rights);
    }
}
