#include <tools/scenarios/runner.h>
#include <tests/host/access.h>

#include <cstdlib>
#include <new>
#include <thread>

namespace { thread_local bool reject_allocations = false; }
void* operator new(std::size_t size) {
    if (reject_allocations) throw std::bad_alloc();
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main(int argc, char** argv) {
#if !defined(__unix__) && !defined(__APPLE__)
    return 77;
#else
    namespace Living = CE::LivingWorld;
    namespace Scenarios = Living::Scenarios;
    namespace Script = CE::Tools::Console::Scenario;
    if (argc != 3) return 2;
    const auto root = std::filesystem::temp_directory_path() /
        ("cheryl-scenario-allocation-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        std::filesystem::path root;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
    } cleanup{root};
    try {
        if (!std::filesystem::create_directory(root)) return 3;
        for (const bool authoring : {false, true}) {
            CE::Workers::Pool pool(1);
            auto io = std::make_shared<Scenarios::IoService>(pool.make_group());
            Scenarios::Config config;
            config.script_root = argv[1]; config.asset_root = argv[2]; config.output_root = root;
            config.script = authoring ? "authoring.console" : "heal.console";
            config.run_id = authoring ? "allocation-authoring" : "allocation";
            config.build_id = "allocation-source-case";
            const auto expected_health = authoring ? 17u : 20u;
            config.io = io;
            Scenarios::Job job(config);
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (job.disposition() == Script::Disposition::Running && std::chrono::steady_clock::now() < deadline) {
                job.step();
                if (job.fixture()) {
                    const auto target = job.fixture()->host->actor(job.fixture()->access, job.fixture()->host->entity(2));
                    if (target && target->health == expected_health) break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (!job.fixture()) return 4;
            const auto* fixture = job.fixture();
            if (fixture->host->actor(fixture->access, fixture->host->entity(2))->health != expected_health || job.disposition() != Script::Disposition::Running) return 5;
            reject_allocations = true;
            while (job.disposition() == Script::Disposition::Running && std::chrono::steady_clock::now() < deadline) {
                job.step(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            reject_allocations = false;
            if (job.batch_exit() == 0 || job.disposition() != Script::Disposition::Incomplete || job.durable()) return 6;
            if (fixture->host->actor(fixture->access, fixture->host->entity(2))->health != expected_health ||
                fixture->host->actor(fixture->access, fixture->host->entity(1))->reserve != 7 ||
                Living::HostTestAccess::image(*fixture->host).record_count != 1) return 7;
            io->settle();
            if (std::filesystem::exists(root / config.run_id / "complete")) return 8;
        }
        return 0;
    } catch (...) { reject_allocations = false; return 99; }
#endif
}
