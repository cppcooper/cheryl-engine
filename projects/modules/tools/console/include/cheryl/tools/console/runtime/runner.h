#pragma once

#include <functional>
#include <cheryl/tools/console/runtime/io.h>

namespace CE::Tools::Console::Runtime {
    /** Host-owned isolated state and newly constructed channel Sessions.
     * report_fields is a trusted host-only JSON member fragment, including its
     * leading comma, or empty. Never interpolate unescaped user input, reserved
     * report keys, or arbitrary script text; use Runtime::quote for strings.
     */
    class Fixture {
    public:
        virtual ~Fixture() = default;
        virtual Core::Registry& registry() noexcept = 0;
        virtual Script::Channels channels() noexcept = 0;
        virtual std::string report_fields() const { return {}; }
    };

    struct Config {
        std::filesystem::path script_root, asset_root, output_root;
        std::string script, run_id, build_id;
        std::size_t output_capacity = max_output_bytes;
        std::chrono::milliseconds wall = Script::max_wall_time;
        std::shared_ptr<IoService> io;
        std::string asset;
        std::function<std::unique_ptr<Fixture>(std::string_view name, std::string_view asset)> factory;
    };
    /** Owns an isolated caller Fixture before its borrowing Run. The factory runs
     * on the step caller, never on the physical I/O lane. Registry and distinct,
     * fresh channel Sessions must remain valid until Job destruction. All host
     * progression uses those Sessions; close never waits for physical I/O.
     * The host retains IoService and its external Pool through settle().
     */
    class Job {
        Config config_;
        std::string source_, source_id_, fixture_, asset_id_, diagnostic_;
        IoHandle io_;
        std::unique_ptr<Fixture> host_;
        std::unique_ptr<Script::Run> run_;
        Script::Error validation_error_;
        Script::Disposition disposition_ = Script::Disposition::Running;
        Script::Disposition domain_ = Script::Disposition::Running;
        std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
        std::string pending_snapshot_;
        bool durable_ = false, initialized_ = false, waiting_ = true, snapshot_pending_ = false;
        std::size_t planned_assertions_ = 0;
        std::string render(bool final) const;
        void initialize(std::string source, std::string asset);
        void persist(bool final);
        void sink_failure() noexcept;
    public:
        explicit Job(Config);
        Job(const Job&) = delete;
        Job& operator=(const Job&) = delete;
        ~Job();
        Script::Disposition step() noexcept;
        void close() noexcept;
        Script::Disposition disposition() const noexcept { return disposition_; }
        bool durable() const noexcept { return durable_; }
        int batch_exit() const noexcept;
        std::string_view diagnostic() const noexcept { return diagnostic_; }
        std::string_view source_id() const noexcept { return source_id_; }
        const Fixture* fixture() const noexcept { return host_.get(); }
        std::span<const Script::NamedResult> results() const noexcept;
        std::span<const Script::Assertion> assertions() const noexcept;
    };
}
