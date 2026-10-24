#include "regional-checks.h"
#include "regional-host.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {
    using namespace ProcGen;
    namespace C = ProcGenRegionalChecks;
    void print(const char* label, const RegionalAdvance& r) {
        const auto& s = r.state;
        const auto& l = r.report.water;
        double soil = 0, cloud = 0, vapour = 0, moment = 0;
        const auto w = s.weather().baseline().descriptor().domain.extents()[0];
        for (std::size_t i = 0; i < s.soil().size(); ++i) {
            soil += s.soil()[i].water_mm;
            cloud += s.weather().water()[i].cloud_mm;
            vapour += s.weather().water()[i].vapour_mm;
            moment += s.weather().water()[i].vapour_mm * static_cast<double>(i % w);
        }
        std::cout << label << ": ticks=" << r.report.from_tick << ".." << r.report.to_tick << " model_s/tick=" << r.report.seconds_per_tick
                  << " soil_sum_mm=" << soil << " cloud_sum_mm=" << cloud
                  << " vapour_centre_x_cells=" << (vapour == 0 ? 0 : moment / vapour) << " work=" << s.usage().work
                  << " payload_bytes=" << s.usage().storage_bytes << '\n';
        std::cout << "  water_m3: initial=" << l.initial_m3 << " final=" << l.final_m3 << " external=" << l.replenished_m3
                  << " finite_to_air=" << l.finite_evaporation_m3 << " rain_internal=" << l.precipitation_m3
                  << " infiltration=" << l.infiltration_m3 << " soil_to_air=" << l.soil_evaporation_m3
                  << " overflow_export=" << l.overflow_export_m3 << " residual=" << l.residual_m3 << " tolerance=" << l.tolerance_m3
                  << '\n';
    }
    RegionWindow region(const RegionalState& s, Index x, Index y, Index w, Index h) {
        return {x, y, w, h, s.weather().baseline().descriptor().context.frame};
    }
}
int main() {
    unsigned passed = 0;
    try {
        std::cout << std::fixed << std::setprecision(10);
        const auto literal = C::literal();
        const auto tiny = advance_regional(literal, 1, regional_hard_limits).value();
        static_cast<void>(C::inspect(literal, tiny));
        print("Literal cloud4mm,soil0.25/cap1mm", tiny);
        ++passed;
        ProcGenRegionalHost::Application app(C::generated());
        const auto baseline = app.state().weather().baseline();
        const auto west_window = region(app.state(), 0, 0, 5, 6), east_window = region(app.state(), 3, 1, 5, 4);
        const auto sleeping = observe_region(app.state(), east_window, regional_hard_limits).value();
        std::array<ProcGenRegionalHost::Marker, 2> saved_markers{};
        RegionalReport storm_report{};
        auto limits = regional_hard_limits;
        limits.snapshots = 2;
        auto history = [&]() {
            auto pre = app.save(limits);
            saved_markers[0] = pre.marker;
            auto storm = advance_regional(app.state(), 10, limits).value();
            static_cast<void>(C::inspect(app.state(), storm));
            print("After storm", storm);
            storm_report = storm.report;
            app.adopt(std::move(storm));
            auto after = app.save(limits);
            saved_markers[1] = after.marker;
            const std::array images{std::move(pre.checkpoint), std::move(after.checkpoint)};
            return retain_regional_history(images, baseline, limits).value();
        }();
        std::cout << "Retained history:2 snapshots, ticks=" << history.snapshots()[0].weather.tick() << '/'
                  << history.snapshots()[1].weather.tick() << " bytes=" << history.usage().storage_bytes
                  << " fields=" << history.usage().field_values << "; peak4 checkpoint images while copying, retained2; no wire format.\n";
        if (!(history.snapshots()[0].soil[1].water_mm == 0 && history.snapshots()[1].soil[1].water_mm > 0))
            throw std::logic_error("missing retained wetness");
        ++passed;
        auto resumed = ProcGenRegionalHost::Application::restore(history.snapshots()[1], saved_markers[1], baseline, limits);
        if (!C::same(app.state(), resumed.state()) || !resumed.ignore_old(storm_report) || !resumed.ignore_old(storm_report) ||
            resumed.marker().wet_alerts != saved_markers[1].wet_alerts)
            throw std::logic_error("restore redelivered an old host alert");
        RegionalAdvance duplicate{resume_regional(history.snapshots()[1], baseline, limits).value(), storm_report};
        bool rejected = false;
        try {
            resumed.adopt(std::move(duplicate));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        if (!rejected || resumed.marker().wet_alerts != saved_markers[1].wet_alerts || !C::same(app.state(), resumed.state()))
            throw std::logic_error("old completed adoption changed restored host");
        const auto uninterrupted = advance_regional(app.state(), 6, limits).value();
        auto split = advance_regional(resumed.state(), 2, limits).value();
        resumed.adopt(std::move(split));
        auto tail = advance_regional(resumed.state(), 4, limits).value();
        if (!C::same(uninterrupted.state, tail.state))
            throw std::logic_error("resume/grouping drift");
        ++passed;
        // Application chooses the uninterrupted branch as its one authoritative owner.
        for (int i = 0; i < 3; ++i) {
            auto next = advance_regional(app.state(), 10, limits).value();
            static_cast<void>(C::inspect(app.state(), next));
            print(i == 2 ? "Clouds cleared, soil retained" : "Transported storm", next);
            app.adopt(std::move(next));
        }
        double wet = 0, cloud = 0;
        for (auto x : app.state().soil())
            wet += x.water_mm;
        for (auto x : app.state().weather().water())
            cloud += x.cloud_mm;
        if (!(wet > 1 && cloud < 1e-6))
            throw std::logic_error("missing after-storm memory");
        ++passed;
        const auto west = observe_region(app.state(), west_window, limits).value(),
                   east = observe_region(app.state(), east_window, limits).value();
        Index overlap = 0;
        for (const auto& a : west.cells)
            for (const auto& b : east.cells)
                if (a.cell == b.cell) {
                    if (a != b)
                        throw std::logic_error("region overlap drift");
                    ++overlap;
                }
        if (overlap != 8 || west.tick != east.tick || sleeping.tick != 36 || east.tick != 76)
            throw std::logic_error("region frame/sleep policy");
        std::cout << "Two independently bounded regions:west[0,5)x[0,6),east[3,8)x[1,5); overlap=" << overlap
                  << " exact owned cells at tick=" << east.tick << "; sleeping observation frozen at tick=" << sleeping.tick
                  << ", wake explicitly samples current owner.\n";
        ++passed;
        auto dry = advance_regional(app.state(), 10, limits).value();
        static_cast<void>(C::inspect(app.state(), dry));
        print("Bounded drying interval", dry);
        double later = 0;
        for (auto s : dry.state.soil())
            later += s.water_mm;
        if (!(later < wet && later > 0))
            throw std::logic_error("missing bounded drying");
        app.adopt(std::move(dry));
        ++passed;
        if (!app.ignore_old(storm_report) || history.snapshots()[0].soil[1].water_mm != 0)
            throw std::logic_error("snapshot/event mutation");
        std::cout
            << "Host save policy:weather+soil+last_applied_tick+wet_alert count adopted together; threshold infiltration>1m3; old report/observations cannot deliver again. Last marker="
            << app.marker().last_applied_tick << " alerts=" << app.marker().wet_alerts
            << ". No arbitrary external exactly-once guarantee.\n"
            << "outer_tick_soil_weather schema/model/numerics1; actual U8 8x6/pitch3/-80..120,10m ticks,2000m cells,40N,southward rows.\n"
            << "Soil cap2mm, drying0.000001mm/s; rain enters soil/overflow once, soil drying returns water to vapour. Signals are game proxies, not ecology/fire/flood physics.\nRegional consumer: "
            << passed << " passed, 0 failed, 0 skipped.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Regional consumer failed after " << passed << ": " << e.what() << '\n';
        return 1;
    }
}
