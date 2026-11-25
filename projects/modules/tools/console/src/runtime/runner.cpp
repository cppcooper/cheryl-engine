#include <cheryl/tools/console/runtime/runner.h>

#include <stdexcept>
#include <algorithm>

namespace CE::Tools::Console::Runtime {
    Fixture::~Fixture() = default;

    namespace {
        std::string fixture_name(std::string_view source) {
            bool version = false;
            while (!source.empty()) {
                const auto end = source.find('\n');
                auto line = source.substr(0, end);
                if (line.ends_with('\r')) line.remove_suffix(1);
                const auto first = line.find_first_not_of(" \t");
                if (first != std::string_view::npos && line[first] != '#') {
                    auto parsed = Core::parse(line);
                    if (!parsed || parsed->tokens.size() != 2) break;
                    const auto& tokens = parsed->tokens;
                    if (!version) {
                        if (tokens[0].text != "script.version" || tokens[1].text != "1") break;
                        version = true;
                    } else {
                        if (tokens[0].text == "fixture.load") return tokens[1].text;
                        break;
                    }
                }
                if (end == std::string_view::npos) break;
                source.remove_prefix(end + 1);
            }
            throw std::runtime_error("Missing fixture identity");
        }
        std::string_view stage(Core::Stage value) {
            switch (value) {
            case Core::Stage::Queued: return "queued";
            case Core::Stage::Pending: return "pending";
            case Core::Stage::Succeeded: return "succeeded";
            case Core::Stage::Refused: return "refused";
            case Core::Stage::Failed: return "failed";
            case Core::Stage::Cancelled: return "cancelled";
            }
            return "unknown";
        }
    }
    Job::Job(Config config) : config_(std::move(config)) {
        try {
            if (!config_.factory) throw std::runtime_error("Isolated host factory is required");
            if (!config_.io) throw std::runtime_error("Host-owned scenario I/O service is required");
            io_ = config_.io->admit({config_.script_root, config_.asset_root, config_.output_root,
                config_.script, config_.run_id, config_.build_id, config_.output_capacity, config_.asset});
            if (!io_) throw std::runtime_error("Scenario I/O admission is closed or full");
        } catch (const std::exception& error) {
            diagnostic_ = error.what(); disposition_ = Script::Disposition::Invalid;
        }
    }
    void Job::initialize(std::string source, std::string asset) {
        initialized_ = true;
        try {
            source_ = std::move(source);
            source_id_ = content_id(source_);
            fixture_ = fixture_name(source_);
            if (!asset.empty()) asset_id_ = content_id(asset);
            host_ = config_.factory(fixture_, asset);
            if (!host_) throw std::runtime_error("Host factory refused the isolated fixture");
            const auto channels = host_->channels();
            auto plan = Script::prepare(host_->registry(), channels, source_);
            if (!plan) {
                validation_error_ = plan.error();
                diagnostic_ = "Validation line " + std::to_string(plan.error().line) + ", code " +
                    std::to_string(static_cast<int>(plan.error().failure.code));
                domain_ = Script::Disposition::Invalid;
                persist(true); return;
            }
            planned_assertions_ = plan->assertions();
            if (planned_assertions_ == 0)
                throw std::runtime_error("A batch requires at least one explicit assertion");
            run_ = std::make_unique<Script::Run>(std::move(*plan), host_->registry(), config_.wall);
            persist(false);
        } catch (const std::exception& error) {
            diagnostic_ = error.what(); domain_ = Script::Disposition::Invalid;
            try { persist(true); } catch (...) { sink_failure(); }
        } catch (...) { sink_failure(); }
    }
    Job::~Job() { close(); }
    std::span<const Script::NamedResult> Job::results() const noexcept { return run_ ? run_->results() : std::span<const Script::NamedResult>{}; }
    std::span<const Script::Assertion> Job::assertions() const noexcept { return run_ ? run_->assertions() : std::span<const Script::Assertion>{}; }
    std::string Job::render(bool final) const {
        const auto& error = run_ ? run_->error() : validation_error_;
        std::size_t executed = 0;
        for (const auto& assertion : assertions()) if (assertion.executed) ++executed;
        std::string json = "{\"schema\":1,\"final\":" + std::string(final ? "true" : "false") + ",\"disposition\":" + quote(label(domain_)) +
            ",\"run\":" + quote(config_.run_id) + ",\"source\":" + quote(config_.script) + ",\"source_id\":" + quote(source_id_) +
            ",\"build_id\":" + quote(config_.build_id) + ",\"fixture\":" + quote(fixture_) + ",\"asset_id\":" + quote(asset_id_) +
            ",\"context_mode\":\"StrictChannels\"" +
            (host_ ? host_->report_fields() : std::string{}) + ",\"planned_assertions\":" + std::to_string(planned_assertions_) +
            ",\"attempted_assertions\":" + std::to_string(assertions().size()) + ",\"executed_assertions\":" + std::to_string(executed) +
            ",\"skipped_assertions\":" + std::to_string(planned_assertions_ - executed) + ",\"steps\":" + std::to_string(run_ ? run_->steps() : 0) +
            ",\"diagnostic\":" + quote(diagnostic_) + ",\"error_line\":" + std::to_string(error.line) +
            ",\"error_code\":" + std::to_string(static_cast<int>(error.failure.code)) +
            ",\"error_offset\":" + std::to_string(error.failure.offset) +
            ",\"error_length\":" + std::to_string(error.failure.length) + ",\"results\":[";
        bool first = true;
        for (const auto& named : results()) {
            if (!first) json += ',';
            first = false;
            json += "{\"name\":" + quote(named.name) + ",\"line\":" + std::to_string(named.line) +
                ",\"route\":" + quote(named.route == Script::Route::Gameplay ? "Gameplay" : "Diagnostic") +
                ",\"request_id\":" + std::to_string(named.request_id) +
                ",\"available\":" + (named.available ? "true" : "false") + ",\"stage\":" + quote(stage(named.result.stage)) +
                ",\"failure\":" + std::to_string(static_cast<int>(named.result.failure.code)) +
                ",\"offset\":" + std::to_string(named.result.failure.offset) + ",\"length\":" + std::to_string(named.result.failure.length) + ",\"value\":";
            json += named.available && named.result.value.type == named.schema.type ? typed(named.result.value, named.schema) : "null";
            json += '}';
        }
        json += "],\"assertions\":["; first = true;
        for (const auto& assertion : assertions()) {
            if (!first) json += ',';
            first = false;
            json += "{\"line\":" + std::to_string(assertion.line) + ",\"reference\":" + quote(assertion.reference) + ",\"executed\":" +
                (assertion.executed ? "true" : "false") + ",\"passed\":" + (assertion.passed ? "true" : "false") +
                ",\"expected\":" + typed(assertion.expected, assertion.schema) + ",\"actual\":" +
                (assertion.executed ? typed(assertion.actual, assertion.schema) : "null") + '}';
        }
        return json + "]}";
    }
    void Job::persist(bool final) {
        pending_snapshot_ = render(final);
        if (pending_snapshot_.size() > Script::max_file_bytes) throw std::runtime_error("Required report exceeds reader limit");
        snapshot_pending_ = true;
    }
    void Job::sink_failure() noexcept {
        if (io_.close() == IoStatus::Complete) { disposition_ = domain_; durable_ = true; return; }
        try {
            if (diagnostic_.empty()) diagnostic_ = "Scenario report sink failed; retained effects are not rolled back";
        } catch (...) {}
        if (run_) run_->close(Script::Disposition::Incomplete);
        disposition_ = Script::Disposition::Incomplete; durable_ = false;
    }
    Script::Disposition Job::step() noexcept {
        if (disposition_ != Script::Disposition::Running) return disposition_;
        try {
            if (waiting_) {
                auto completion = io_.poll();
                if (completion.status == IoStatus::Complete) {
                    durable_ = true; disposition_ = domain_; io_.close(); return disposition_;
                }
                if (completion.status == IoStatus::Failed) {
                    if (completion.error) std::rethrow_exception(completion.error);
                    throw std::runtime_error("Scenario I/O lane failed");
                }
                if (completion.status == IoStatus::Cancelled) { close(); return disposition_; }
                if (completion.status != IoStatus::Pending) {
                    waiting_ = false;
                    if (completion.status == IoStatus::Input) initialize(std::move(completion.source), std::move(completion.asset));
                }
            }
            if (disposition_ != Script::Disposition::Running) return disposition_;
            if (std::chrono::steady_clock::now() - started_ >= config_.wall) {
                if (diagnostic_.empty()) diagnostic_ = "Scenario wall deadline expired; retained effects are not rolled back";
                sink_failure(); return disposition_;
            }
            if (waiting_) return disposition_;
            if (!snapshot_pending_) {
                domain_ = run_->step();
                persist(domain_ != Script::Disposition::Running);
            }
            const auto submitted = io_.submit(pending_snapshot_, domain_ != Script::Disposition::Running, source_id_);
            if (submitted == IoSubmit::Accepted) { snapshot_pending_ = false; pending_snapshot_.clear(); waiting_ = true; }
            else if (submitted == IoSubmit::Closed) close();
        } catch (const std::exception& error) {
            try { if (diagnostic_.empty()) diagnostic_ = error.what(); } catch (...) {}
            if (!initialized_) { disposition_ = Script::Disposition::Invalid; io_.close(); }
            else sink_failure();
        } catch (...) { sink_failure(); }
        return disposition_;
    }
    void Job::close() noexcept {
        if (disposition_ != Script::Disposition::Running) return;
        const auto completion = io_.close();
        if (completion == IoStatus::Complete) { disposition_ = domain_; durable_ = true; return; }
        if (run_) run_->close();
        disposition_ = domain_ == Script::Disposition::Running || domain_ == Script::Disposition::Passed ?
            Script::Disposition::Cancelled : domain_;
    }
    int Job::batch_exit() const noexcept { return disposition_ == Script::Disposition::Passed && durable_ && planned_assertions_ != 0 &&
        assertions().size() == planned_assertions_ && std::ranges::all_of(assertions(),
            [](const auto& assertion) { return assertion.executed && assertion.passed; }) ? 0 : 1; }
}
