#pragma once

#include <cheryl/tools/console/parser.h>
#include <cheryl/tools/console/session.h>
#include <chrono>
#include <optional>

namespace CE::Tools::Console::Scenario {
    inline constexpr std::size_t max_file_bytes = 65536, max_lines = 256;
    inline constexpr std::size_t max_statements = 128, max_names = 32, max_steps = 256;
    inline constexpr auto max_wall_time = std::chrono::seconds(10);
    enum class Disposition { Running, Passed, Failed, Invalid, Incomplete, Cancelled };
    struct Error { std::size_t line = 0; Failure failure; };
    template<class Type> using Checked = std::expected<Type, Error>;
    namespace Detail { struct PlanData; struct RunData; }
    enum class Route { Gameplay, Diagnostic };
    /** Host-issued Context values and borrowed, newly constructed accepting Sessions.
     * Each Session must match its Context exactly and remain exclusively owned by
     * the run. Registry, Sessions, and issuing authorities outlive Plan and Run.
     * Distinct channels require distinct Sessions and !Context::same_scope.
     */
    struct Channel { Context context; Session& session; };
    struct Channels { Channel gameplay; std::optional<Channel> diagnostic; };

    class Plan;
    CE_CONSOLE_API Checked<Plan> prepare(const Registry&, const Context&, std::string_view source);
    CE_CONSOLE_API Checked<Plan> prepare(const Registry&, const Channels&, std::string_view source);

    /** Version 1: script.version 1, fixture.load name, command args -> name,
     * refuse command args -> name, error Owner command args -> name,
     * retry $name -> copy, await $name max_steps=N -> outcome,
     * expect $name.field == literal, and finish. Blank and whole # lines are ignored.
     * Command quoting/escaping follows Console::parse; nested test.run is forbidden.
     * References are backward-only, typed, and symbolic until their producer runs.
     * The caller retains the serialized registry/provider and issued contexts
     * from validation through execution. No handlers run during validation.
     */
    class CE_TYPE_API Plan {
        std::shared_ptr<const Detail::PlanData> data_;
        explicit Plan(std::shared_ptr<const Detail::PlanData> data) : data_(std::move(data)) {}
        friend Checked<Plan> prepare(const Registry&, const Context&, std::string_view);
        friend Checked<Plan> prepare(const Registry&, const Channels&, std::string_view);
        friend class Run;
    public:
        CE_CONSOLE_API std::string_view fixture() const noexcept;
        CE_CONSOLE_API std::size_t assertions() const noexcept;
    };
    CE_CONSOLE_API Checked<Plan> prepare(const Registry&, const Context&, std::string_view source);
    /** Strict developer-test routing: ordinary commands use Gameplay only;
     * observe COMMAND, observe refuse COMMAND, and observe error Owner COMMAND
     * select the optional Query-only Diagnostic channel. No denial fallback.
     * Scalar references may cross channels, but handle-bearing schemas may not.
     * Dynamic arguments pin the exact registration without fabricated values;
     * real producer values bind later in the consumer's original Context.
     * A strict Plan starts once, including copies; use fresh Sessions for a new run.
     * Live, accepting, matching, distinct, empty channels are checked twice.
     * The single-Context overload remains legacy and grants no gameplay guarantee.
     */
    CE_CONSOLE_API Checked<Plan> prepare(const Registry&, const Channels&, std::string_view source);
    struct NamedResult {
        std::string name;
        std::size_t line = 0;
        std::uint64_t request_id = 0;
        Schema schema;
        Result result;
        bool available = false;
        Route route = Route::Gameplay;
    };
    struct Assertion {
        std::size_t line = 0;
        std::string reference;
        Schema schema;
        Value expected, actual;
        bool executed = false, passed = false;
    };
    /** A run borrows its registry and dedicated Sessions, which must outlive it.
     * step() pumps at most one Session once and never sleeps. Snapshots own their values;
     * Session remains authoritative if copying fails. close() follows Session's
     * finite entered-work policy and refreshes retained effects, never reissuing.
     * A terminal result is not a durable machine report; the host owns that sink.
     */
    class CE_TYPE_API Run {
        std::unique_ptr<Detail::RunData> data_;
    public:
        CE_CONSOLE_API Run(Plan, Registry&, Session&, std::chrono::milliseconds wall = max_wall_time);
        /** Runs a strict Plan using exactly its retained borrowed Sessions.
         * Rechecks live/accepting/matching/empty channels; step pumps at most one selected
         * Session, and close refreshes and closes both without replay.
         */
        CE_CONSOLE_API Run(Plan, Registry&, std::chrono::milliseconds wall = max_wall_time);
        Run(const Run&) = delete;
        Run& operator=(const Run&) = delete;
        CE_CONSOLE_API ~Run();
        CE_CONSOLE_API Disposition step() noexcept;
        CE_CONSOLE_API void close(Disposition reason = Disposition::Cancelled) noexcept;
        CE_CONSOLE_API Disposition disposition() const noexcept;
        CE_CONSOLE_API const Error& error() const noexcept;
        CE_CONSOLE_API std::span<const NamedResult> results() const noexcept;
        CE_CONSOLE_API std::span<const Assertion> assertions() const noexcept;
        CE_CONSOLE_API std::size_t steps() const noexcept; ///< Total Session::pump calls, bounded by max_steps.
    };
}
