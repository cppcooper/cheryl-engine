#include <tools/scenarios/runner.h>

#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    if (argc != 7) {
        std::cerr << "usage: living-scenarios SCRIPT_ROOT ASSET_ROOT OUTPUT_ROOT SCRIPT RUN_ID BUILD_ID\n";
        return 2;
    }
    try {
        CE::Workers::Pool pool(1);
        auto io = std::make_shared<CE::LivingWorld::Scenarios::IoService>(pool.make_group());
        CE::LivingWorld::Scenarios::Config config;
        config.script_root = argv[1]; config.asset_root = argv[2]; config.output_root = argv[3];
        config.script = argv[4]; config.run_id = argv[5]; config.build_id = argv[6];
        config.io = io;
        CE::LivingWorld::Scenarios::Job job(std::move(config));
        const auto deadline = std::chrono::steady_clock::now() + CE::Tools::Console::Scenario::max_wall_time;
        while (job.disposition() == CE::Tools::Console::Scenario::Disposition::Running && std::chrono::steady_clock::now() < deadline) {
            job.step();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        job.close();
        if (job.batch_exit()) std::cerr << "Scenario did not pass: " << job.diagnostic() << '\n';
        io->close_admission();
        io->settle();
        pool.shutdown();
        return job.batch_exit();
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}
