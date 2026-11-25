#include <cheryl/tools/console/parser.h>
#include <cheryl/tools/console/session.h>
#include <cheryl/tools/console/scenario.h>

#include <cstdlib>
#include <limits>
#include <new>

namespace {
    bool reject_allocations = false;
    std::size_t remaining_allocations = std::numeric_limits<std::size_t>::max();
}
void* operator new(std::size_t size) {
    if (reject_allocations || !remaining_allocations) throw std::bad_alloc();
    if (remaining_allocations != std::numeric_limits<std::size_t>::max()) --remaining_allocations;
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
    namespace Core = CE::Tools::Console;
    class Apply final : public Core::Work {
        unsigned& effects_;
    public:
        explicit Apply(unsigned& effects) : effects_(effects) { result.value = Core::Value::unsigned_integer(0); }
        void step() noexcept override {
            ++effects_; result.value = Core::Value::unsigned_integer(effects_); result.stage = Core::Stage::Succeeded;
        }
        void close() noexcept override { result.stage = Core::Stage::Cancelled; }
    };
}
int main() {
    try {
        Core::Authority authority; auto context = authority.issue(0); Core::Registry registry; Core::Session session(context);
        unsigned effects = 0;
        Core::Schema text, output; output.type = Core::Type::Unsigned;
        Core::Descriptor descriptor{"apply", "Allocation boundary.", "test", Core::Kind::Action, Core::Timing::OwnerImmediate,
            0, {{"text", text}}, output};
        auto registration = registry.add<std::string>(descriptor, [&effects](const Core::Context&, std::string) {
            return std::make_shared<Apply>(effects);
        });
        if (!registration) return 1;
        std::vector<Core::Value> arguments{Core::Value::text(std::string(128, 'x'))};
        reject_allocations = true;
        auto resolved = registry.resolve("apply", context);
        if (!resolved) return 2;
        auto copied = resolved;
        auto request = registry.bind(*copied, std::move(arguments));
        reject_allocations = false;
        if (!request) return 2;
        reject_allocations = true;
        auto failed = session.submit(1, *request);
        reject_allocations = false;
        if (failed || failed.error().code != Core::Code::Allocation || session.retained() || effects) return 3;
        if (!session.submit(1, *request)) return 4;
        reject_allocations = true;
        auto pumped = session.pump(); auto retained = session.lookup(1); auto retry = session.submit(1, *request);
        reject_allocations = false;
        if (!pumped || !retained || !retry || (*retained)->stage != Core::Stage::Failed ||
            (*retained)->failure.code != Core::Code::Allocation || effects) return 5;
        if (!session.submit(2, *request) || !session.pump() || effects != 1) return 6;
        reject_allocations = true;
        retained = session.lookup(2); retry = session.submit(2, *request); session.close();
        reject_allocations = false;
        if (!retained || !retry || (*retained)->stage != Core::Stage::Succeeded || effects != 1) return 7;
        auto consume_descriptor = descriptor;
        consume_descriptor.name = "consume"; consume_descriptor.arguments = {{"count", output}};
        unsigned consumers = 0;
        auto consumer = registry.add<std::uint64_t>(consume_descriptor, [&consumers](const Core::Context&, std::uint64_t) {
            return std::make_shared<Apply>(consumers);
        });
        if (!consumer) return 8;
        Core::Session strict_session(context);
        Core::Scenario::Channels channels{{context, strict_session}, std::nullopt};
        const std::string script = "script.version 1\nfixture.load allocation\napply " + std::string(128, 'x') +
            " -> first\nconsume $first -> second\nexpect $second == 1\nfinish";
        auto valid = Core::Scenario::prepare(registry, channels, script);
        if (!valid) return 9;
        bool preparation_completed = false;
        std::size_t preparation_failures = 0;
        for (std::size_t budget = 0; budget != 1024; ++budget) {
            remaining_allocations = budget;
            auto prepared = Core::Scenario::prepare(registry, channels, script);
            remaining_allocations = std::numeric_limits<std::size_t>::max();
            if (effects != 1 || consumers || strict_session.retained()) return 10;
            if (prepared) { preparation_completed = true; break; }
            if (prepared.error().failure.code != Core::Code::Allocation) return 11;
            ++preparation_failures;
        }
        if (!preparation_completed || preparation_failures < 2) return 12;
        bool constructed = false;
        std::size_t construction_failures = 0;
        for (std::size_t budget = 0; budget != 1024; ++budget) {
            remaining_allocations = budget;
            try {
                Core::Scenario::Run run(*valid, registry);
                remaining_allocations = std::numeric_limits<std::size_t>::max();
                constructed = true;
            } catch (const std::bad_alloc&) {
                remaining_allocations = std::numeric_limits<std::size_t>::max();
                ++construction_failures;
                if (!strict_session.accepting() || strict_session.retained()) return 13;
            }
            if (constructed) break;
        }
        if (!constructed || construction_failures < 2 || effects != 1 || consumers) return 14;
        {
            Core::Session literal_session(context);
            Core::Scenario::Channels literal_channels{{context, literal_session}, std::nullopt};
            auto literal_plan = Core::Scenario::prepare(registry, literal_channels, script);
            if (!literal_plan) return 19;
            Core::Scenario::Run literal_run(*literal_plan, registry);
            reject_allocations = true;
            auto literal_step = literal_run.step();
            reject_allocations = false;
            if (literal_step != Core::Scenario::Disposition::Incomplete ||
                literal_run.error().failure.code != Core::Code::Allocation || literal_run.error().line != 3 ||
                literal_session.retained() || effects != 1 || consumers) return 20;
            if (literal_run.step() != Core::Scenario::Disposition::Incomplete || effects != 1 || consumers) return 21;
        }
        Core::Session runtime_session(context);
        Core::Scenario::Channels runtime_channels{{context, runtime_session}, std::nullopt};
        auto runtime_plan = Core::Scenario::prepare(registry, runtime_channels, script);
        if (!runtime_plan) return 15;
        Core::Scenario::Run run(*runtime_plan, registry);
        if (run.step() != Core::Scenario::Disposition::Running || effects != 2) return 16;
        reject_allocations = true;
        auto failed_step = run.step();
        reject_allocations = false;
        if (failed_step != Core::Scenario::Disposition::Incomplete || run.error().failure.code != Core::Code::Allocation ||
            effects != 2 || consumers || runtime_session.retained() != 1) return 17;
        auto applied = runtime_session.lookup(1);
        if (!applied || (*applied)->stage != Core::Stage::Succeeded) return 18;
        return 0;
    } catch (...) { reject_allocations = false; return 99; }
}
