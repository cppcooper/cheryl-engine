#include "host.h"
#include <cheryl/tools/console/runtime/diagnostics.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

namespace {
    namespace Core = Independent::Core;
    namespace Runtime = Independent::Runtime;
    namespace Script = Independent::Script;

    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }
    std::uint64_t effective(const Core::Result& result) {
        return std::get<std::uint64_t>(std::get<Core::Value::Children>(result.value.data)[1].data);
    }
    void group_observation(CE::Workers::Group group) {
        Core::Authority authority;
        const auto context = authority.issue(1);
        Core::Registry registry;
        Runtime::EngineObservations observations(registry, "sdk.io", group, {}, 1);
        const auto descriptor = registry.describe("sdk.io.group", context);
        require(descriptor && descriptor->kind == Core::Kind::Query, "Worker observation is not Query-only");
        require(!registry.describe("sdk.io.logging_backend", context), "Absent logger produced invented diagnostics");
        const auto request = Core::bind_text(registry, context, "sdk.io.group");
        require(request.has_value(), "Worker observation did not bind");
        Core::Session session(context);
        require(session.submit(1, *request).has_value() && session.pump().has_value(), "Worker observation failed");
        const auto result = session.lookup(1);
        require(result && (*result)->stage == Core::Stage::Succeeded, "Worker observation has no retained result");
        const auto& fields = std::get<Core::Value::Children>((*result)->value.data);
        const auto status = group.status();
        require(std::get<std::uint64_t>(fields[0].data) == status.domain &&
            std::get<std::uint64_t>(fields[2].data) == status.accepted, "Observation is not the actual logical I/O group");
    }
    void same_handlers() {
        auto host = std::make_shared<Independent::Host>();
        const auto described = host->registry().describe("simulation.step_limit.set", host->context());
        require(described && described->kind == Core::Kind::Setting &&
            described->timing == Core::Timing::OwnerImmediate &&
            described->arguments[0].schema.unsigned_max == 16, "Setting discovery lost range or timing");
        auto typed = host->registry().bind("simulation.step_limit.set", {Core::Value::unsigned_integer(6)}, host->context());
        require(typed.has_value(), "Typed setting did not bind");
        require(host->session().submit(1, *typed).has_value(), "Typed setting not retained");
        require(host->simulation().step_limit == 4, "Setting mutated before owner progression");
        require(host->session().pump().has_value(), "Typed progression failed");
        auto retained = host->session().lookup(1);
        require(retained && (*retained)->stage == Core::Stage::Succeeded && effective(**retained) == 6,
            "Typed setting did not reach actual owner");
        require(host->session().submit(1, *typed).has_value(), "Exact retained retry failed");
        require(host->session().pump().has_value() && host->simulation().writes == 1, "Retry repeated the effect");
        auto invalid = host->registry().bind("simulation.step_limit.set", {Core::Value::unsigned_integer(17)}, host->context());
        require(!invalid && invalid.error().code == Core::Code::Range, "Typed range refusal missing");

        auto interactive = std::make_shared<Independent::Host>();
        Core::Presentation::Model model(interactive);
        require(model.submit("simulation.step_limit.set 6"), "Text setting did not bind");
        model.pump();
        require(interactive->simulation().step_limit == host->simulation().step_limit, "Text and typed handlers disagree");
        require(!model.submit("simulation.step_limit.set 17") && interactive->simulation().step_limit == 6,
            "Invalid text changed the actual owner");
        require(model.submit("simulation.step_limit.read"), "Text discovery read failed");
        model.pump();
        require(model.entries().back().result.stage == Core::Stage::Succeeded &&
            effective(model.entries().back().result) == 6, "Model lost typed result");
        require(model.complete("simulation.step_limit.r") == "simulation.step_limit.read ", "Completion did not use host registry");
        require(Core::Presentation::escaped(model.scope()).find("&lt;owner-thread&gt;") != std::string::npos,
            "Untrusted scope is not escaped");
        model.close();
        require(!model.accepting() && effective(model.entries().back().result) == 6, "Close discarded retained results");
    }
    void run_case(Runtime::Config config, const char* script, const char* run, Script::Disposition expected,
        bool should_pass, bool durable, std::uint64_t expected_limit) {
        config.script = script; config.run_id = run;
        Runtime::Job job(config);
        const auto deadline = std::chrono::steady_clock::now() + Script::max_wall_time;
        while (job.disposition() == Script::Disposition::Running && std::chrono::steady_clock::now() < deadline) {
            job.step(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        job.close();
        require(job.disposition() == expected, "Unexpected job disposition");
        require((job.batch_exit() == 0) == should_pass, "Batch exit hid failed or empty assertions");
        require(job.durable() == durable, "Report finalization mismatch");
        if (const auto* host = dynamic_cast<const Independent::Host*>(job.fixture())) {
            require(host->simulation().step_limit == expected_limit, "Whole-file validation allowed partial mutation");
            require(host->simulation().writes == (should_pass ? 2u : 0u), "Invalid input or exact retry repeated an owner effect");
        }
        else require(!durable, "Finalized job has no isolated host");
        if (durable) {
            const auto report = Runtime::read_source(config.output_root, std::string{run} + "/report.json");
            require(!report.empty() && report.find("\"final\":true") != std::string::npos, "Empty or unfinished report");
            const auto transcript = Runtime::read_source(config.output_root, std::string{run} + "/transcript.txt");
            const auto marker = Runtime::read_source(config.output_root, std::string{run} + "/complete");
            const auto expected_marker = std::string{run} + '\n' + std::string{job.source_id()} + '\n' +
                Runtime::content_id(config.build_id) + '\n' + Runtime::content_id(report) + '\n' +
                Runtime::content_id(transcript) + '\n';
            require(marker == expected_marker, "Missing or invalid complete marker");
            require(Runtime::completed_report(config.output_root, run, job.source_id(), config.build_id) == should_pass,
                "Successful report reader hid failure");
        }
        if (should_pass) {
            require(!job.assertions().empty(), "Empty assertion suite passed");
            for (const auto& assertion : job.assertions()) require(assertion.executed && assertion.passed, "Skipped assertion passed");
        }
    }
}

int main(int argc, char** argv) {
    try {
        require(argc == 3, "usage: sdk-console FIXTURES OUTPUT_ROOT");
        same_handlers();
        const auto unique = "independent-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto output = std::filesystem::absolute(argv[2]) / unique;
        std::filesystem::create_directories(output);
        CE::Workers::Pool pool(1);
        const auto group = pool.make_group();
        auto io = std::make_shared<Runtime::IoService>(group);
        group_observation(group);
        Runtime::Config config;
        config.script_root = std::filesystem::absolute(argv[1]); config.output_root = output;
        config.build_id = "independent-installed-console"; config.io = io; config.factory = Independent::fixture;
        run_case(config, "success.console", "success", Script::Disposition::Passed, true, true, 0);
        run_case(config, "failure.console", "failure", Script::Disposition::Failed, false, true, 4);
        run_case(config, "empty.console", "empty", Script::Disposition::Invalid, false, true, 4);
        run_case(config, "invalid.console", "invalid", Script::Disposition::Invalid, false, true, 4);
        run_case(config, "diagnostic-write.console", "diagnostic-write", Script::Disposition::Invalid, false, true, 4);
        run_case(config, "missing.console", "missing", Script::Disposition::Invalid, false, false, 4);
#if defined(_WIN32)
        run_case(config, "NUL", "device-input", Script::Disposition::Invalid, false, false, 4);
        run_case(config, "success.console", "COM1", Script::Disposition::Invalid, false, false, 4);
#endif
        auto broken = config;
        broken.output_root = output / "nonexistent";
        run_case(broken, "success.console", "broken-output", Script::Disposition::Invalid, false, false, 4);
        io->close_admission(); io->settle(); pool.shutdown();
        std::cout << "Independent host assertions and finalized reports: " << output << '\n';
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
