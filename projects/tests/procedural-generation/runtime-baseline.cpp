// Finite real-owner timing witness, with optional actual Engine buffers and
// retained scratch. No global allocator override or alternative weather solver.
#include "regional-checks.h"
#include "moisture-checks.h"
#include "runtime-admission.h"
#ifdef PROCGEN_ENGINE_MEMORY
#include <core/resources/objects/object-reservation.hpp>
#include <core/logging/process-lifetime.h>
#endif
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
    struct Metrics {
        std::uint64_t checkout_calls = 0, checkout_bytes = 0;
        std::uint64_t backing_calls = 0, backing_bytes = 0, metadata_calls = 0, metadata_bytes = 0, control_calls = 0, control_bytes = 0;
        std::uint64_t copied_baseline = 0, copied_weather = 0, copied_soil = 0;
        std::size_t retained_scratch = 0, engine_backing_live = 0, engine_metadata_live = 0;
    };
    struct Sample {
        double update, thread_cpu, adopt, read, destroy_old, destroy_report, age;
        Index work, tick, substeps;
        Metrics metrics{};
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
    void memory_snapshot(const char* phase, const Case& c) {
#ifdef PROCGEN_ENGINE_MEMORY
        auto& manager = CE::Memory::ExactManager::get();
        const auto state = manager.diagnostics();
        const auto backing = CE::Memory::Detail::backing_allocations.snapshot();
        const auto metadata = CE::Memory::Detail::metadata_allocations.snapshot();
        const auto controls = CE::Memory::Detail::control_allocations.snapshot();
        std::cout << "{\"kind\":\"engine_memory\",\"case\":\"" << c.name << "\",\"phase\":\"" << phase
                  << "\",\"domain\":" << state.domain << ",\"backing_bytes\":" << state.total_bytes
                  << ",\"pooled_bytes\":" << state.available_bytes << ",\"backing_owners\":" << state.owners
                  << ",\"split_ranges\":" << state.ranges << ",\"native_backing_peak_bytes\":" << backing.peak
                  << ",\"metadata_live_requested_bytes\":" << metadata.live << ",\"metadata_peak_requested_bytes\":" << metadata.peak
                  << ",\"selected_control_live_requested_bytes\":" << controls.live << ",\"selected_control_peak_requested_bytes\":" << controls.peak
                  << "}\n";
#else
        static_cast<void>(phase);
        static_cast<void>(c);
#endif
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
                  << ",\"copied_baseline_cell_bytes\":" << s.metrics.copied_baseline
                  << ",\"copied_weather_cell_face_bytes\":" << s.metrics.copied_weather
                  << ",\"copied_soil_cell_bytes\":" << s.metrics.copied_soil
#ifdef PROCGEN_ENGINE_MEMORY
                  << ",\"engine_checkout_calls\":" << s.metrics.checkout_calls << ",\"engine_checkout_requested_bytes\":" << s.metrics.checkout_bytes
                  << ",\"engine_backing_calls\":" << s.metrics.backing_calls << ",\"engine_backing_bytes\":" << s.metrics.backing_bytes
                  << ",\"engine_metadata_calls\":" << s.metrics.metadata_calls << ",\"engine_metadata_requested_bytes\":" << s.metrics.metadata_bytes
                  << ",\"engine_control_calls\":" << s.metrics.control_calls << ",\"engine_control_requested_bytes\":" << s.metrics.control_bytes
                  << ",\"retained_scratch_capacity_bytes\":" << s.metrics.retained_scratch
                  << ",\"engine_backing_live_bytes\":" << s.metrics.engine_backing_live
                  << ",\"engine_metadata_live_bytes\":" << s.metrics.engine_metadata_live
#else
                  << ",\"engine_backing_calls\":null,\"engine_metadata_calls\":null"
#endif
                  << "}\n";
    }
    Sample step(std::optional<RegionalState>& current, const Case& c, Index& work, double& consumed, WeatherWorkspace* scratch) {
        auto budgets = regional_hard_limits;
        budgets.work = std::min(budgets.work, total_work_limit - work);
        const auto copied_baseline = CopyMetrics::baseline_bytes, copied_weather = CopyMetrics::weather_bytes, copied_soil = CopyMetrics::soil_bytes;
#ifdef PROCGEN_ENGINE_MEMORY
        const auto checkout_calls = CE::Memory::Detail::byte_checkout_calls.load();
        const auto checkout_bytes = CE::Memory::Detail::byte_checkout_requested_bytes.load();
        const auto backing = CE::Memory::Detail::backing_allocations.snapshot();
        const auto metadata = CE::Memory::Detail::metadata_allocations.snapshot();
        const auto controls = CE::Memory::Detail::control_allocations.snapshot();
#endif
        const auto cpu = cpu_ms();
        const auto begin = Clock::now();
        auto next = scratch ? advance_regional(*current, c.ticks, budgets, *scratch) : advance_regional(*current, c.ticks, budgets);
        const auto end = Clock::now();
        const auto end_cpu = cpu_ms();
        Metrics metrics;
        metrics.copied_baseline = CopyMetrics::baseline_bytes - copied_baseline;
        metrics.copied_weather = CopyMetrics::weather_bytes - copied_weather;
        metrics.copied_soil = CopyMetrics::soil_bytes - copied_soil;
        metrics.retained_scratch = scratch ? scratch->retained_bytes() : 0;
#ifdef PROCGEN_ENGINE_MEMORY
        metrics.checkout_calls = CE::Memory::Detail::byte_checkout_calls.load() - checkout_calls;
        metrics.checkout_bytes = CE::Memory::Detail::byte_checkout_requested_bytes.load() - checkout_bytes;
        const auto after_backing = CE::Memory::Detail::backing_allocations.snapshot();
        const auto after_metadata = CE::Memory::Detail::metadata_allocations.snapshot();
        const auto after_controls = CE::Memory::Detail::control_allocations.snapshot();
        metrics.backing_calls = after_backing.calls - backing.calls;
        metrics.backing_bytes = after_backing.bytes - backing.bytes;
        metrics.metadata_calls = after_metadata.calls - metadata.calls;
        metrics.metadata_bytes = after_metadata.bytes - metadata.bytes;
        metrics.control_calls = after_controls.calls - controls.calls;
        metrics.control_bytes = after_controls.bytes - controls.bytes;
        metrics.engine_backing_live = after_backing.live;
        metrics.engine_metadata_live = after_metadata.live;
#endif
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
        result.metrics = metrics;
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
int main(int argc, char** argv) {
    try {
#ifdef PROCGEN_ENGINE_MEMORY
        CE::Logging::ProcessLifetime logging;
#endif
        const bool reuse = argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--reuse");
        if (argc > 2 || (argc == 2 && !reuse && std::string_view(argv[1]) != "--no-reuse"))
            throw std::runtime_error("expected --reuse or --no-reuse");
        std::cout << std::setprecision(17);
        std::cout
            << "{\"kind\":\"plan\",\"backend\":\"see_backend_record\",\"timer\":\"steady_clock/CLOCK_THREAD_CPUTIME_ID\",\"cold\":1,\"warmup\":1,\"warm_samples\":3,\"wall_limit_s\":30,\"charged_work_limit\":600000000,\"stop_case_above_ms\":50,\"nearest_rank_percentiles\":true,\"allocation_and_copy_measurement\":\"instrumented CE backing, collection nodes, selected control blocks and copied cell payloads; excludes cold standard allocations and descriptor buffers\"}\n";
        constexpr std::array cases{
            Case{"small", 32, 32, 60, 1, 2, false}, Case{"representative", 64, 48, 120, 1, 2, false},
            Case{"stress", 128, 128, 1000, 1, 4, false}, Case{"expensive", 128, 128, 1000, 2, 4, false},
            Case{"refused", 128, 128, 1000, 1, 4, true}
        };
        std::cout << "{\"kind\":\"backend\",\"engine\":"
#ifdef PROCGEN_ENGINE_MEMORY
                  << "true"
#else
                  << "false"
#endif
                  << ",\"reuse_scratch\":" << (reuse ? "true" : "false") << "}\n";
        const auto campaign = Clock::now();
        Index work = 0;
        double consumed = 0;
        for (const auto& c : cases) {
            if (elapsed(campaign, Clock::now()) > total_seconds_limit * 1000 || work >= total_work_limit)
                throw std::runtime_error("bounded campaign exhausted");
            const auto setup_start = Clock::now();
            std::optional<RegionalState> current(setup(c));
#ifdef PROCGEN_ENGINE_MEMORY
            std::shared_ptr<WeatherWorkspace> scratch_owner;
            {
                CE::Memory::Objects::Pool<WeatherWorkspace> facade;
                CE::Memory::Allocators::ObjectPoolAllocator<WeatherWorkspace> allocator(facade.release_context());
                CE::Memory::Objects::Reservation<WeatherWorkspace> reservation(1, allocator);
                scratch_owner = reservation.emplace(0);
            }
            WeatherWorkspace* owned_scratch = scratch_owner.get();
#else
            WeatherWorkspace scratch_value;
            WeatherWorkspace* owned_scratch = &scratch_value;
#endif
            WeatherWorkspace* scratch = reuse ? owned_scratch : nullptr;
            const auto setup_end = Clock::now();
            memory_snapshot("setup", c);
            const auto req = regional_requirements(current->weather()).value();
            std::cout << "{\"kind\":\"setup\",\"case\":\"" << c.name << "\",\"ms\":" << elapsed(setup_start, setup_end)
                      << ",\"modeled_state_bytes\":" << req.state_storage_bytes
                      << ",\"modeled_peak_candidate_bytes\":" << req.advance_storage_bytes << "}\n";
            const auto drain = [&]() {
                const auto prior = checksum(*current);
#ifdef PROCGEN_ENGINE_MEMORY
                const auto checkouts = CE::Memory::Detail::byte_checkout_calls.load();
#endif
                RuntimeAllowance allowance{std::min(runtime_update_work, total_work_limit - work), 1};
                auto admitted = advance_runtime(*current, 1, allowance, *owned_scratch);
                const bool expected = c.w <= 64 && c.h <= 48 && c.seconds <= 120 && c.scales <= 2;
                if (static_cast<bool>(admitted) != expected || checksum(*current) != prior)
                    throw std::runtime_error("runtime profile admission/preservation mismatch");
                if (admitted) {
                    work += admitted->state.usage().work;
                    static_cast<void>(ProcGenRegionalChecks::inspect(*current, *admitted));
                } else {
                    work += admitted.error().work;
#ifdef PROCGEN_ENGINE_MEMORY
                    if (CE::Memory::Detail::byte_checkout_calls.load() != checkouts)
                        throw std::runtime_error("profile refusal allocated Engine storage");
#endif
                }
                std::cout << "{\"kind\":\"runtime_profile\",\"case\":\"" << c.name
                          << "\",\"accepted\":" << (admitted ? "true" : "false")
                          << ",\"charged_work\":" << (admitted ? admitted->state.usage().work : admitted.error().work)
                          << ",\"prior_state_preserved\":true}\n";
                admitted = std::unexpected(RegionalFailure{FailureKind::invalid_input, RegionalReason::weather, 0, {}});
                const auto begin = Clock::now();
                current.reset();
                const auto end = Clock::now();
                std::cout << "{\"kind\":\"final_owner_drain\",\"case\":\"" << c.name << "\",\"ms\":" << elapsed(begin, end) << "}\n";
                memory_snapshot("state_drained_workspace_retained", c);
#ifdef PROCGEN_ENGINE_MEMORY
                const auto workspace_begin = Clock::now();
                scratch_owner.reset();
                std::cout << "{\"kind\":\"workspace_drain\",\"case\":\"" << c.name
                          << "\",\"ms\":" << elapsed(workspace_begin, Clock::now()) << "}\n";
                memory_snapshot("state_and_workspace_drained", c);
#endif
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
            const auto cold = step(current, c, work, consumed, scratch);
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
                const auto warm = step(current, c, work, consumed, scratch);
                raw(c, "warmup", i, warm);
                if (warm.update > stop_update_ms)
                    stopped = true;
            }
            std::size_t count = 0;
            if (!stopped)
                for (; count < warm_samples; ++count) {
                    if (elapsed(campaign, Clock::now()) > total_seconds_limit * 1000)
                        throw std::runtime_error("wall allowance exhausted");
                    samples[count] = step(current, c, work, consumed, scratch);
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
        memory_snapshot("campaign_drained", cases.back());
        std::cout << "{\"kind\":\"finished\",\"charged_work\":" << work << ",\"wall_ms\":" << elapsed(campaign, Clock::now())
                  << ",\"checksum\":" << consumed << ",\"engine_backed_acceptance\":false}\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Baseline campaign failed: " << e.what() << '\n';
        return 1;
    }
}
