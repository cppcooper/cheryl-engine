#include "scenario-internal.h"

#include <algorithm>
#include <stdexcept>

namespace CE::Tools::Console::Scenario {
    namespace Detail {
        struct RunData {
            std::shared_ptr<const PlanData> plan;
            Registry& registry;
            Session& session;
            Session* diagnostic_session;
            std::vector<NamedResult> results;
            std::vector<Assertion> assertions;
            std::vector<std::optional<BoundRequest>> requests;
            std::chrono::steady_clock::time_point deadline;
            Disposition disposition = Disposition::Running;
            Error error;
            std::size_t cursor = 0, steps = 0, wait_steps = 0;
            std::array<std::uint64_t, 2> next_id{};
            RunData(std::shared_ptr<const PlanData> plan, Registry& registry, Session& session,
                    std::chrono::milliseconds wall, Session* diagnostic_session = nullptr)
                : plan(std::move(plan)), registry(registry), session(session), diagnostic_session(diagnostic_session),
                  deadline(std::chrono::steady_clock::now() + wall) {
                results.reserve(this->plan->names.size()); requests.resize(this->plan->names.size());
                assertions.reserve(this->plan->assertions);
                for (const auto& name : this->plan->names)
                    results.push_back({name.text, name.line, 0, name.schema, {}, false, name.route});
            }
            Session& selected(Route route) const noexcept {
                return route == Route::Gameplay ? session : *diagnostic_session;
            }
            void close_sessions() noexcept {
                session.close();
                if (diagnostic_session) diagnostic_session->close();
            }
            void refresh() {
                for (auto& result : results) if (result.request_id) {
                    auto retained = selected(result.route).lookup(result.request_id);
                    if (!retained) continue;
                    result.result = **retained; result.available = true;
                }
            }
            bool expected(std::size_t index) const noexcept {
                const auto& result = results[index];
                const auto& name = plan->names[index];
                return result.available && result.result.stage == name.expected &&
                    result.result.failure.code == name.error;
            }
            void stop(Disposition reason, Failure failure, std::size_t line) noexcept {
                disposition = reason; error = {line, failure};
                close_sessions();
                try { refresh(); } catch (...) {
                    disposition = Disposition::Incomplete;
                    if (error.failure.code == Code::None) error.failure = {Code::Allocation};
                }
            }
            void stop(Disposition reason, Code code, std::size_t line) noexcept { stop(reason, Failure{code}, line); }
            Failure mismatch(std::size_t index) const noexcept {
                const auto failure = results[index].result.failure;
                return plan->routed && failure.code == Code::None ? Failure{Code::Owner} : failure;
            }
            bool pump(Route route, std::size_t line) {
                if (steps == max_steps) { stop(Disposition::Incomplete, Code::Limit, line); return false; }
                ++steps;
                auto pumped = selected(route).pump();
                if (!pumped) { stop(Disposition::Incomplete, pumped.error(), line); return false; }
                refresh(); return true;
            }
        };
    }
    Run::Run(Plan plan, Registry& registry, Session& session, std::chrono::milliseconds wall) {
        if (plan.data_->routed || wall <= std::chrono::milliseconds::zero() || wall > max_wall_time || session.retained() ||
            !plan.data_->context.same_scope(session.context())) throw std::invalid_argument("Scenario requires fresh matching session and bounded time");
        data_ = std::make_unique<Detail::RunData>(std::move(plan.data_), registry, session, wall);
    }
    Run::Run(Plan plan, Registry& registry, std::chrono::milliseconds wall) {
        if (!plan.data_->routed || plan.data_->registry != &registry ||
            wall <= std::chrono::milliseconds::zero() || wall > max_wall_time || !Detail::fresh_channels(*plan.data_))
            throw std::invalid_argument("Scenario requires fresh exact routed channels and registry");
        auto* gameplay = plan.data_->gameplay_session;
        auto* diagnostic = plan.data_->diagnostic_session;
        data_ = std::make_unique<Detail::RunData>(std::move(plan.data_), registry, *gameplay, wall, diagnostic);
        data_->plan->started = true;
    }
    Run::~Run() { if (data_->disposition == Disposition::Running) close(); }
    Disposition Run::step() noexcept {
        auto& run = *data_;
        if (run.disposition != Disposition::Running) return run.disposition;
        const auto& statement = run.plan->statements[run.cursor];
        try {
            if (run.plan->routed && (!run.plan->context.live() ||
                (run.plan->diagnostic && !run.plan->diagnostic->live()))) {
                run.stop(Disposition::Incomplete, Code::Expired, statement.line); return run.disposition;
            }
            if (std::chrono::steady_clock::now() >= run.deadline) {
                run.stop(Disposition::Incomplete, Code::Limit, statement.line); return run.disposition;
            }
            if (statement.kind == Detail::Kind::Command) {
                auto& session = run.selected(statement.route);
                auto& output = run.results[statement.output];
                if (!output.request_id) {
                    if (statement.bound && !statement.resolved) run.requests[statement.output] = statement.bound;
                    else {
                        const auto& context = run.plan->selected(statement.route);
                        if (!statement.resolved) {
                            auto described = run.registry.describe(statement.descriptor.name, context);
                            if (!described || *described != statement.descriptor) {
                                run.stop(Disposition::Failed, Code::Unregistered, statement.line); return run.disposition;
                            }
                        }
                        std::vector<Value> arguments;
                        for (const auto& input : statement.inputs) {
                            if (!input.reference) arguments.push_back(input.literal);
                            else {
                                const auto& producer = run.results[input.reference->name];
                                if (!terminal(producer.result.stage) || !run.expected(input.reference->name)) {
                                    run.stop(Disposition::Failed, Code::Arguments, statement.line); return run.disposition;
                                }
                                const auto* value = Detail::value(producer, *input.reference);
                                if (!value) { run.stop(Disposition::Failed, Code::Type, statement.line); return run.disposition; }
                                arguments.push_back(*value);
                            }
                        }
                        auto bound = statement.resolved ? run.registry.bind(*statement.resolved, std::move(arguments)) :
                            run.registry.bind(statement.descriptor.name, std::move(arguments), context);
                        if (!bound) {
                            run.stop(statement.bound ? Disposition::Incomplete : Disposition::Failed, bound.error(), statement.line);
                            return run.disposition;
                        }
                        run.requests[statement.output] = std::move(*bound);
                    }
                    const auto id = ++run.next_id[static_cast<std::size_t>(statement.route)];
                    auto submitted = session.submit(id, *run.requests[statement.output]);
                    if (!submitted) { run.stop(Disposition::Incomplete, submitted.error(), statement.line); return run.disposition; }
                    output.request_id = id;
                }
                if (!run.pump(statement.route, statement.line)) return run.disposition;
                if (terminal(output.result.stage) && !run.expected(statement.output)) {
                    run.stop(Disposition::Failed, run.mismatch(statement.output), statement.line); return run.disposition;
                }
            } else if (statement.kind == Detail::Kind::Await || statement.kind == Detail::Kind::Retry) {
                const auto& source = run.results[statement.source];
                if (!source.request_id || !run.requests[statement.source]) {
                    run.stop(Disposition::Failed, Code::Arguments, statement.line); return run.disposition;
                }
                if (statement.kind == Detail::Kind::Retry) {
                    auto retried = run.selected(source.route).submit(source.request_id, *run.requests[statement.source]);
                    if (!retried) { run.stop(Disposition::Failed, retried.error(), statement.line); return run.disposition; }
                    run.refresh();
                } else {
                    run.refresh();
                    if (!terminal(source.result.stage)) {
                        if (run.wait_steps == statement.budget) {
                            run.stop(Disposition::Incomplete, Code::Limit, statement.line); return run.disposition;
                        }
                        ++run.wait_steps;
                        if (!run.pump(source.route, statement.line)) return run.disposition;
                        if (!terminal(source.result.stage)) return run.disposition;
                    }
                    if (!run.expected(statement.source)) {
                        run.stop(Disposition::Failed, run.mismatch(statement.source), statement.line); return run.disposition;
                    }
                }
                auto& output = run.results[statement.output];
                output.request_id = source.request_id; output.result = source.result; output.available = true;
                run.requests[statement.output] = run.requests[statement.source];
            } else if (statement.kind == Detail::Kind::Expect) {
                Assertion assertion{statement.line, statement.spelling, statement.reference.schema, statement.literal, {}, false, false};
                const auto* actual = Detail::value(run.results[statement.reference.name], statement.reference);
                if (actual) { assertion.actual = *actual; assertion.executed = true; assertion.passed = assertion.actual == assertion.expected; }
                run.assertions.push_back(std::move(assertion));
                if (!run.assertions.back().passed) { run.stop(Disposition::Failed, Code::Type, statement.line); return run.disposition; }
            } else {
                run.refresh();
                for (std::size_t index = 0; index < run.results.size(); ++index) {
                    if (!run.results[index].available || !terminal(run.results[index].result.stage)) {
                        run.stop(Disposition::Incomplete, Code::Limit, statement.line); return run.disposition;
                    }
                    if (!run.expected(index)) {
                        run.stop(Disposition::Failed, run.plan->routed ? run.mismatch(index) : Failure{Code::Owner}, statement.line);
                        return run.disposition;
                    }
                }
                if (run.assertions.size() != run.plan->assertions || run.assertions.empty()) {
                    run.stop(Disposition::Incomplete, Code::Schema, statement.line); return run.disposition;
                }
                run.disposition = Disposition::Passed;
                run.close_sessions();
                return run.disposition;
            }
            ++run.cursor; run.wait_steps = 0;
        } catch (const std::bad_alloc&) { run.stop(Disposition::Incomplete, Code::Allocation, statement.line); }
        catch (...) { run.stop(Disposition::Incomplete, Code::Owner, statement.line); }
        return run.disposition;
    }
    void Run::close(Disposition reason) noexcept {
        if (data_->disposition == Disposition::Running)
            data_->stop(reason == Disposition::Passed || reason == Disposition::Running ? Disposition::Cancelled : reason,
                Code::Cancelled, data_->plan->statements[data_->cursor].line);
        else if (reason == Disposition::Incomplete) data_->disposition = Disposition::Incomplete;
    }
    Disposition Run::disposition() const noexcept { return data_->disposition; }
    const Error& Run::error() const noexcept { return data_->error; }
    std::span<const NamedResult> Run::results() const noexcept { return data_->results; }
    std::span<const Assertion> Run::assertions() const noexcept { return data_->assertions; }
    std::size_t Run::steps() const noexcept { return data_->steps; }
}
