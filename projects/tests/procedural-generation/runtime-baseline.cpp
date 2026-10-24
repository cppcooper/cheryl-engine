// Pre-migration U15 timing witness. Uses real production owners/solver and fixed
// benchmark storage; no global allocator override or alternative weather solver.
#include "regional-checks.h"
#include "moisture-checks.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace {
    using namespace ProcGen;
    using Clock = std::chrono::steady_clock;
    static_assert(Clock::is_steady);
    constexpr Index total_work_limit = 600'000'000;
    constexpr double total_seconds_limit = 30, stop_update_ms = 50;
    constexpr std::size_t warmups = 1, warm_samples = 3;
    struct Case {
        const char* name;
        Index w, h, seconds, ticks, scales;
        bool refused;
    };
    struct Sample {
        double update, thread_cpu, adopt, read, destroy_old, destroy_report, age;
        Index work, tick, substeps;
    };
    double elapsed(Clock::time_point begin, Clock::time_point end) {
        return std::chrono::duration<double, std::milli>(end - begin).count();
    }
    double cpu_ms() {
        timespec ts{};
        if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0)
            throw std::runtime_error("thread CPU timer unavailable");
        return static_cast<double>(ts.tv_sec) * 1000 + static_cast<double>(ts.tv_nsec) / 1e6;
    }
    RegionalState setup(const Case& c) {
        const Index cells = c.w * c.h;
        const auto elevation =
            generate_elevation(std::array<Index, 2>{c.w, c.h}, {42, 0x67656f6772617068, 7}, {7, -80, 120}, elevation_hard_limits).value();
        // Fixed2.25MiB bounded setup arrays, not new uninstrumented runtime heaps.
        std::array<ClimateSurface, 16384> surface{};
        std::array<MoistureCell, 16384> water{};
        std::array<FlowCell, 16384> flow{};
        std::array<SoilCell, 16384> soil{};
        for (Index i = 0; i < cells; ++i) {
            const auto n = static_cast<std::size_t>(i);
            const auto x = i % c.w;
            surface[n].land_response_seconds = 3600;
            surface[n].water_response_seconds = 14400;
            surface[n].roughness_metres = x == 0 ? .01 : .8;
            surface[n].replenished_water_fraction = x == 0 ? 1 : 0;
            if (x == 1) {
                water[n].vapour_mm = 9;
                water[n].cloud_mm = .5;
            }
            flow[n].east_m_s = 1;
            soil[n].capacity_mm = 2;
            soil[n].evaporation_mm_per_second = .000001;
        }
        const auto middle = static_cast<std::size_t>(cells / 2 + c.w / 2);
        surface[middle].finite_water_fraction = 1;
        water[middle].finite_surface_mm = .02;
        const auto context = ProcGenMoistureChecks::context(1000, 1000, c.seconds);
        const auto baseline =
            derive_climate(elevation, std::span(surface).first(static_cast<std::size_t>(cells)), {1, 1}, context, 36, climate_hard_limits)
                .value();
        for (std::size_t i = 0; i < static_cast<std::size_t>(cells); ++i)
            flow[i].surface_kelvin = 280 - .0065 * baseline.cells()[i].z_metres;
        WeatherModel model;
        model.forcing.semantic = {42, 0x77696e64, 7};
        model.forcing.count = c.scales;
        model.forcing.origin_east_metres = 10000;
        model.forcing.origin_north_metres = 20000;
        model.forcing.scales = {WeatherScale{16, 8, .25}, WeatherScale{32, 16, .1}, WeatherScale{64, 32, .05}, WeatherScale{128, 64, .02}};
        auto weather = start_weather(
                           baseline, std::span(water).first(static_cast<std::size_t>(cells)),
                           std::span(flow).first(static_cast<std::size_t>(cells)), model, weather_hard_limits
        )
                           .value();
        return start_regional(weather, std::span(soil).first(static_cast<std::size_t>(cells)), regional_hard_limits).value();
    }
    double checksum(const RegionalState& s) {
        double result = 0;
        for (std::size_t i = 0; i < s.soil().size(); ++i) {
            const auto q = s.weather().water()[i];
            const auto f = s.weather().flow()[i];
            result += q.vapour_mm + q.cloud_mm + q.finite_surface_mm + s.soil()[i].water_mm + f.surface_kelvin + f.potential + f.east_m_s +
                      f.north_m_s;
        }
        return result;
    }
    void raw(const Case& c, const char* phase, std::size_t index, const Sample& s) {
        std::cout << "{\"kind\":\"sample\",\"case\":\"" << c.name << "\",\"phase\":\"" << phase << "\",\"sample\":" << index
                  << ",\"grid\":[" << c.w << ',' << c.h << "],\"ticks\":" << c.ticks << ",\"model_seconds_per_tick\":" << c.seconds
                  << ",\"forcing_scales\":" << c.scales << ",\"work\":" << s.work << ",\"model_tick\":" << s.tick
                  << ",\"substeps_per_tick\":" << s.substeps << ",\"complete_candidate_ms\":" << s.update
                  << ",\"update_thread_cpu_ms\":" << s.thread_cpu << ",\"move_adoption_ms\":" << s.adopt
                  << ",\"two_region_read_and_destroy_ms\":" << s.read << ",\"old_state_destroy_ms\":" << s.destroy_old
                  << ",\"report_destroy_ms\":" << s.destroy_report
                  << ",\"synchronous_total_ms\":" << s.update + s.adopt + s.read + s.destroy_old + s.destroy_report
                  << ",\"sample_age_ms\":" << s.age
                  << ",\"update_only_ms\":null,\"measured_allocation_calls\":null,\"measured_copy_bytes\":null}\n";
    }
    Sample step(std::optional<RegionalState>& current, const Case& c, Index& work, double& consumed) {
        auto budgets = regional_hard_limits;
        budgets.work = std::min(budgets.work, total_work_limit - work);
        const auto cpu = cpu_ms();
        const auto begin = Clock::now();
        auto next = advance_regional(*current, c.ticks, budgets);
        const auto end = Clock::now();
        const auto end_cpu = cpu_ms();
        if (!next)
            throw std::runtime_error(
                "admitted baseline case refused: regional reason=" + std::to_string(static_cast<int>(next.error().reason))
            );
        work += next->state.usage().work;
        static_cast<void>(ProcGenRegionalChecks::inspect(*current, *next)); // independent checks OUTSIDE timer
        consumed += checksum(next->state);
        Sample result{
            elapsed(begin, end),
            end_cpu - cpu,
            0,
            0,
            0,
            0,
            0,
            next->state.usage().work,
            next->state.weather().tick(),
            next->state.weather().usage().substeps_per_tick
        };
        std::optional<RegionalState> old;
        const auto adopt = Clock::now();
        old.emplace(std::move(*current));
        current.reset();
        current.emplace(std::move(next->state));
        const auto published = Clock::now();
        result.adopt = elapsed(adopt, published);
        const auto read = Clock::now();
        {
            const auto& frame = current->weather().baseline().descriptor().context.frame;
            const auto west = observe_region(*current, {0, 0, 4, 4, frame}, regional_hard_limits).value();
            const auto east = observe_region(*current, {2, 0, 4, 4, frame}, regional_hard_limits).value();
            if (west.tick != east.tick)
                throw std::runtime_error("read tick mismatch");
            Index overlap = 0;
            for (const auto& a : west.cells)
                for (const auto& b : east.cells)
                    if (a.cell == b.cell) {
                        if (a != b)
                            throw std::runtime_error("read overlap mismatch");
                        ++overlap;
                    }
            if (overlap != 8)
                throw std::runtime_error("read coverage mismatch");
            consumed += west.cells[0].signals.local_kelvin + east.cells[0].soil.water_mm;
        }
        const auto read_end = Clock::now();
        result.read = elapsed(read, read_end);
        result.age = elapsed(published, read_end);
        const auto destroy = Clock::now();
        old.reset();
        const auto destroyed = Clock::now();
        result.destroy_old = elapsed(destroy, destroyed);
        const auto report = Clock::now();
        next = std::unexpected(RegionalFailure{FailureKind::invalid_input, RegionalReason::weather, 0, {}});
        result.destroy_report = elapsed(report, Clock::now());
        return result;
    }
    void summary(const Case& c, const char* phase, std::span<const Sample> samples, bool complete) {
        std::array<double, warm_samples> values{}, total{};
        for (std::size_t i = 0; i < samples.size(); ++i) {
            values[i] = samples[i].update;
            total[i] = samples[i].update + samples[i].adopt + samples[i].read + samples[i].destroy_old + samples[i].destroy_report;
        }
        std::sort(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(samples.size()));
        std::sort(total.begin(), total.begin() + static_cast<std::ptrdiff_t>(samples.size()));
        std::cout << "{\"kind\":\"summary\",\"case\":\"" << c.name << "\",\"phase\":\"" << phase
                  << "\",\"actual_samples\":" << samples.size()
                  << ",\"planned_samples\":" << (std::string_view(phase) == "cold" ? 1 : warm_samples)
                  << ",\"complete\":" << (complete ? "true" : "false");
        if (complete && !samples.empty()) {
            const auto median = (samples.size() + 1) / 2 - 1;
            std::cout << ",\"candidate_p50_ms\":" << values[median] << ",\"candidate_p95_ms\":" << values[samples.size() - 1]
                      << ",\"candidate_max_ms\":" << values[samples.size() - 1] << ",\"sync_p95_ms\":" << total[samples.size() - 1];
        } else {
            std::cout << ",\"candidate_p50_ms\":null,\"candidate_p95_ms\":null,\"candidate_max_ms\":";
            if (samples.empty())
                std::cout << "null";
            else
                std::cout << values[samples.size() - 1];
            std::cout << ",\"sync_p95_ms\":null";
        }
        std::cout << "}\n";
    }
}
int main() {
    try {
        std::cout << std::setprecision(17);
        std::cout
            << "{\"kind\":\"plan\",\"backend\":\"unchanged_stdlib_U15\",\"timer\":\"steady_clock/CLOCK_THREAD_CPUTIME_ID\",\"cold\":1,\"warmup\":1,\"warm_samples\":3,\"wall_limit_s\":30,\"charged_work_limit\":600000000,\"stop_case_above_ms\":50,\"nearest_rank_percentiles\":true,\"allocation_and_copy_measurement\":\"unavailable: pinned CE dependencies missing; no global allocator interception\"}\n";
        constexpr std::array cases{
            Case{"small", 32, 32, 60, 1, 2, false}, Case{"representative", 64, 48, 120, 1, 2, false},
            Case{"stress", 128, 128, 1000, 1, 4, false}, Case{"expensive", 128, 128, 1000, 2, 4, false},
            Case{"refused", 128, 128, 1000, 1, 4, true}
        };
        const auto campaign = Clock::now();
        Index work = 0;
        double consumed = 0;
        for (const auto& c : cases) {
            if (elapsed(campaign, Clock::now()) > total_seconds_limit * 1000 || work >= total_work_limit)
                throw std::runtime_error("bounded campaign exhausted");
            const auto setup_start = Clock::now();
            std::optional<RegionalState> current(setup(c));
            const auto setup_end = Clock::now();
            const auto req = regional_requirements(current->weather()).value();
            std::cout << "{\"kind\":\"setup\",\"case\":\"" << c.name << "\",\"ms\":" << elapsed(setup_start, setup_end)
                      << ",\"modeled_state_bytes\":" << req.state_storage_bytes
                      << ",\"modeled_peak_candidate_bytes\":" << req.advance_storage_bytes << "}\n";
            const auto drain = [&]() {
                const auto begin = Clock::now();
                current.reset();
                const auto end = Clock::now();
                std::cout << "{\"kind\":\"final_owner_drain\",\"case\":\"" << c.name << "\",\"ms\":" << elapsed(begin, end) << "}\n";
            };
            if (c.refused) {
                auto budgets = regional_hard_limits;
                budgets.substeps_per_tick = 63;
                const auto before = checksum(*current);
                const auto begin = Clock::now();
                const auto rejected = advance_regional(*current, 1, budgets);
                const auto end = Clock::now();
                if (rejected || rejected.error().reason != RegionalReason::substeps || checksum(*current) != before)
                    throw std::runtime_error("expected refusal/preservation failed");
                work += rejected.error().work;
                std::cout
                    << "{\"kind\":\"refusal\",\"case\":\"refused\",\"grid\":[128,128],\"ticks\":1,\"model_seconds_per_tick\":1000,\"forcing_scales\":4,\"allowed_substeps\":63,\"required_substeps\":64,\"work\":"
                    << rejected.error().work << ",\"ms\":" << elapsed(begin, end) << ",\"prior_state_preserved\":true}\n";
                drain();
                continue;
            }
            std::array<Sample, warm_samples> samples{};
            const auto cold = step(current, c, work, consumed);
            raw(c, "cold", 0, cold);
            summary(c, "cold", std::span(&cold, 1), true);
            if (cold.update > stop_update_ms) {
                summary(c, "warm", {}, false);
                drain();
                continue;
            }
            bool stopped = false;
            for (std::size_t i = 0; i < warmups; ++i) {
                if (elapsed(campaign, Clock::now()) > total_seconds_limit * 1000)
                    throw std::runtime_error("wall allowance exhausted");
                const auto warm = step(current, c, work, consumed);
                raw(c, "warmup", i, warm);
                if (warm.update > stop_update_ms)
                    stopped = true;
            }
            std::size_t count = 0;
            if (!stopped)
                for (; count < warm_samples; ++count) {
                    if (elapsed(campaign, Clock::now()) > total_seconds_limit * 1000)
                        throw std::runtime_error("wall allowance exhausted");
                    samples[count] = step(current, c, work, consumed);
                    raw(c, "warm", count, samples[count]);
                    if (samples[count].update > stop_update_ms) {
                        ++count;
                        break;
                    }
                }
            summary(c, "warm", std::span(samples).first(count), count == warm_samples);
            drain();
        }
        if (!std::isfinite(consumed) || consumed == 0)
            throw std::runtime_error("unconsumed output");
        std::cout << "{\"kind\":\"finished\",\"charged_work\":" << work << ",\"wall_ms\":" << elapsed(campaign, Clock::now())
                  << ",\"checksum\":" << consumed << ",\"engine_backed_acceptance\":false}\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Baseline campaign failed: " << e.what() << '\n';
        return 1;
    }
}
