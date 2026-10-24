#include "regional-checks.h"
#include "regional-host.h"
#include "moisture-checks.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <new>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace RegionalAllocation {
    thread_local bool denied = false;
}
void* operator new(std::size_t n) {
    if (RegionalAllocation::denied)
        throw std::bad_alloc();
    if (void* p = std::malloc(n == 0 ? 1 : n))
        return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) {
    return ::operator new(n);
}
void operator delete(void* p) noexcept {
    std::free(p);
}
void operator delete[](void* p) noexcept {
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept {
    std::free(p);
}

namespace {
    using namespace ProcGen;
    namespace C = ProcGenRegionalChecks;
    std::size_t assertions = 0;
    void check(bool value, const char* text, int line) {
        ++assertions;
        if (!value)
            throw std::runtime_error("line " + std::to_string(line) + ": " + text);
    }
#define CHECK(x) check(static_cast<bool>(x), #x, __LINE__)
    void near(double a, double b, double tol = 1e-10) {
        CHECK(std::abs(a - b) <= tol * std::max({1.0, std::abs(a), std::abs(b)}));
    }
    RegionWindow window(const RegionalState& s, Index x, Index y, Index w, Index h) {
        return {x, y, w, h, s.weather().baseline().descriptor().context.frame};
    }
    RegionalState flat(Index w = 1, Index h = 1, Index seconds = 1) {
        const auto b = C::baseline(w, h, seconds);
        const auto weather = start_weather(
                                 b, std::vector<MoistureCell>(static_cast<std::size_t>(w * h)),
                                 std::vector<FlowCell>(static_cast<std::size_t>(w * h)), C::passive(), weather_hard_limits
        )
                                 .value();
        return start_regional(weather, std::vector<SoilCell>(static_cast<std::size_t>(w * h)), regional_hard_limits).value();
    }
    void soil() {
        const auto old = C::literal();
        const auto r = advance_regional(old, 1, regional_hard_limits).value();
        assertions += C::inspect(old, r);
        //4mm cloud, half falls;0.75 infiltrates into free capacity,1.25 exports.
        //Then0.1mm evaporates: cloud2,Q0.1,W0.9, total3+export1.25=4.25.
        near(r.state.soil()[0].water_mm, .9);
        near(r.state.weather().water()[0].cloud_mm, 2);
        near(r.state.weather().water()[0].vapour_mm, .1);
        const auto t = r.report.transfers[0];
        near(t.precipitation_mm, 2);
        near(t.infiltration_mm, .75);
        near(t.overflow_mm, 1.25);
        near(t.evaporation_mm, .1);
        near(r.report.water.initial_m3, 42.5);
        near(r.report.water.final_m3, 30);
        near(r.report.water.overflow_export_m3, 12.5);
        const auto next = advance_regional(r.state, 1, regional_hard_limits).value();
        assertions += C::inspect(r.state, next);
        near(next.state.soil()[0].water_mm, .9);
        near(next.report.transfers[0].overflow_mm, .9);
        auto image = capture_regional(old, regional_hard_limits).value();
        image.soil[0] = {0, 0, 1, 0};
        const auto zero = resume_regional(image, old.weather().baseline(), regional_hard_limits).value();
        const auto none = advance_regional(zero, 1, regional_hard_limits).value();
        near(none.state.soil()[0].water_mm, 0);
        near(none.report.transfers[0].overflow_mm, 2);
        image.soil[0] = {0, 10, .25, 0};
        const auto fraction = resume_regional(image, old.weather().baseline(), regional_hard_limits).value();
        const auto quarter = advance_regional(fraction, 1, regional_hard_limits).value();
        near(quarter.report.transfers[0].infiltration_mm, .5);
        near(quarter.report.transfers[0].overflow_mm, 1.5);
    }
    void drying() {
        auto initial = flat();
        auto image = capture_regional(initial, regional_hard_limits).value();
        image.soil[0] = {.3, 1, 1, .1};
        auto s = resume_regional(image, initial.weather().baseline(), regional_hard_limits).value();
        const auto r = advance_regional(s, 4, regional_hard_limits).value();
        assertions += C::inspect(s, r);
        near(r.state.soil()[0].water_mm, 0);
        near(r.state.weather().water()[0].vapour_mm, .3);
        near(r.report.transfers[0].evaporation_mm, .3);
        near(r.report.transfers[0].precipitation_mm, 0);
        near(r.report.water.overflow_export_m3, 0);
        const auto dry = advance_regional(r.state, 4, regional_hard_limits).value();
        assertions += C::inspect(r.state, dry);
        near(dry.report.transfers[0].evaporation_mm, 0);
    }
    void supply() {
        auto c = ProcGenMoistureChecks::context(100, 100, 1);
        std::array<ClimateSurface, 1> surfaces{};
        surfaces[0].finite_water_fraction = .5;
        surfaces[0].replenished_water_fraction = .5;
        const auto b = ProcGenMoistureChecks::baseline(1, 1, std::array<Height, 1>{0}, surfaces, c);
        auto model = C::passive();
        model.moisture.surface_evaporation_mm_per_second = .1;
        std::array<MoistureCell, 1> q{};
        q[0].finite_surface_mm = .075;
        const auto weather = start_weather(b, q, std::array<FlowCell, 1>{}, model, weather_hard_limits).value();
        const auto s = start_regional(weather, std::array<SoilCell, 1>{{{.2, 1, 1, .1}}}, regional_hard_limits).value();
        const auto r = advance_regional(s, 2, regional_hard_limits).value();
        assertions += C::inspect(s, r);
        near(r.report.water.finite_evaporation_m3, .75);
        near(r.report.water.replenished_m3, 1);
        near(r.state.weather().water()[0].finite_surface_mm, 0);
        near(r.state.weather().water()[0].vapour_mm, .375);
        near(r.state.soil()[0].water_mm, 0);
        near(r.report.water.soil_evaporation_m3, 2);
    }
    void resume() {
        const auto s = C::generated();
        const auto& b = s.weather().baseline();
        const auto direct = advance_regional(s, 6, regional_hard_limits).value();
        const auto first = advance_regional(s, 2, regional_hard_limits).value();
        const auto cp = capture_regional(first.state, regional_hard_limits).value();
        const auto restored = resume_regional(cp, b, regional_hard_limits).value();
        CHECK(C::same(first.state, restored));
        const auto tail = advance_regional(restored, 4, regional_hard_limits).value();
        CHECK(C::same(direct.state, tail.state));
        near(direct.report.water.infiltration_m3, first.report.water.infiltration_m3 + tail.report.water.infiltration_m3);
        near(direct.report.water.soil_evaporation_m3, first.report.water.soil_evaporation_m3 + tail.report.water.soil_evaporation_m3);
        CHECK(cp.weather.model() == restored.weather().model());
        CHECK(cp.weather.baseline().descriptor() == restored.weather().baseline().descriptor());
        CHECK(std::ranges::equal(cp.weather.baseline().cells(), restored.weather().baseline().cells()));
        CHECK(cp.weather.tick() == 38);
        CHECK(direct.state.weather().tick() == 42);
    }
    void history() {
        const auto s = C::literal();
        const auto next = advance_regional(s, 1, regional_hard_limits).value();
        std::array images{capture_regional(s, regional_hard_limits).value(), capture_regional(next.state, regional_hard_limits).value()};
        const auto h = retain_regional_history(images, s.weather().baseline(), regional_hard_limits).value();
        CHECK(h.snapshots().size() == 2);
        CHECK(h.snapshots()[0].soil.data() != images[0].soil.data());
        CHECK(h.snapshots()[0].weather.water().data() != images[0].weather.water().data());
        images[0].soil[0].water_mm = 0;
        near(h.snapshots()[0].soil[0].water_mm, .25);
        auto cap = regional_hard_limits;
        cap.cells = 1;
        cap.snapshots = 2;
        cap.work = h.usage().work;
        cap.storage_bytes = h.usage().storage_bytes;
        cap.field_values = h.usage().field_values;
        CHECK(retain_regional_history(h.snapshots(), s.weather().baseline(), cap));
        for (int k = 0; k < 5; ++k) {
            auto b = cap;
            if (k == 0)
                --b.cells;
            if (k == 1)
                --b.snapshots;
            if (k == 2)
                --b.work;
            if (k == 3)
                --b.storage_bytes;
            if (k == 4)
                --b.field_values;
            CHECK(!retain_regional_history(h.snapshots(), s.weather().baseline(), b));
        }
        std::swap(images[0], images[1]);
        CHECK(!retain_regional_history(images, s.weather().baseline(), regional_hard_limits));
        CHECK(!retain_regional_history({}, s.weather().baseline(), regional_hard_limits));
        std::vector<RegionalCheckpoint> too_many(5, h.snapshots()[0]);
        CHECK(!retain_regional_history(too_many, s.weather().baseline(), regional_hard_limits));
    }
    void regions() {
        const auto initial = C::generated();
        const auto a = window(initial, 0, 0, 5, 6), b = window(initial, 3, 1, 5, 4);
        const auto sleep = observe_region(initial, b, regional_hard_limits).value();
        const auto r = advance_regional(initial, 4, regional_hard_limits).value();
        const auto west = observe_region(r.state, a, regional_hard_limits).value(),
                   east = observe_region(r.state, b, regional_hard_limits).value();
        CHECK(west.tick == east.tick && east.tick == 40);
        Index overlap = 0;
        for (const auto& x : west.cells)
            for (const auto& y : east.cells)
                if (x.cell == y.cell) {
                    CHECK(x == y);
                    ++overlap;
                }
        CHECK(overlap == 8);
        CHECK(sleep.tick == 36);
        CHECK(sleep.cells[0].soil.water_mm == 0);
        const auto wake = observe_region(r.state, b, regional_hard_limits).value();
        CHECK(wake.cells == east.cells);
        CHECK(sleep.cells != wake.cells);
        CHECK(west.source == east.source);
        CHECK(west.weather_model == east.weather_model);
    }
    void signals() {
        auto s = flat();
        auto cp = capture_regional(s, regional_hard_limits).value();
        cp.soil[0] = {.25, 1, 1, 0};
        s = resume_regional(cp, s.weather().baseline(), regional_hard_limits).value();
        const auto o = observe_region(s, window(s, 0, 0, 1, 1), regional_hard_limits).value();
        const auto q = o.cells[0].signals;
        near(q.local_kelvin, 280);
        near(q.column_saturation_proxy, 0);
        near(q.cloud_fog_proxy, 0);
        near(q.dryness_proxy, .75);
        near(q.wind_exposure_m_s, 0);
        near(q.fire_susceptibility_proxy, 0);
        const auto literal = C::literal();
        const auto fog = observe_region(literal, window(literal, 0, 0, 1, 1), regional_hard_limits).value();
        near(fog.cells[0].signals.cloud_fog_proxy, .8);
        auto model = C::passive();
        const auto b = C::baseline();
        std::array<FlowCell, 1> f{};
        f[0].east_m_s = 3;
        std::array<MoistureCell, 1> water{};
        water[0].vapour_mm = 20;
        const auto weather = start_weather(b, water, f, model, weather_hard_limits).value();
        const auto wind = start_regional(weather, std::array<SoilCell, 1>{{{.25, 1, 1, 0}}}, regional_hard_limits).value();
        const auto fire = observe_region(wind, window(wind, 0, 0, 1, 1), regional_hard_limits).value();
        near(fire.cells[0].signals.fire_susceptibility_proxy, .5625);
        near(fire.cells[0].signals.column_saturation_proxy, 1);
    }
    void generated() {
        auto s = C::generated();
        const auto before = capture_regional(s, regional_hard_limits).value();
        double rain = 0;
        RegionalState after_storm = s;
        for (int interval = 0; interval < 4; ++interval) {
            const auto r = advance_regional(s, 10, regional_hard_limits).value();
            assertions += C::inspect(s, r);
            rain += r.report.water.precipitation_m3;
            if (interval == 0)
                after_storm = r.state;
            s = r.state;
        }
        double soil = 0, cloud = 0;
        for (auto q : s.soil())
            soil += q.water_mm;
        for (auto q : s.weather().water())
            cloud += q.cloud_mm;
        CHECK(rain > 0);
        CHECK(cloud < 1e-6);
        CHECK(soil > 1);
        CHECK(after_storm.soil()[1].water_mm > 0);
        CHECK(before.soil[1].water_mm == 0);
        const auto dry = advance_regional(s, 10, regional_hard_limits).value();
        assertions += C::inspect(s, dry);
        double later = 0;
        for (auto q : dry.state.soil())
            later += q.water_mm;
        CHECK(later < soil);
        CHECK(later > 0);
        CHECK(s.weather().tick() == 76);
        auto cosmetic = before; // Weather model is immutable; build equivalent alternate start with only cosmetic key changed.
        auto model = before.weather.model();
        model.forcing.cosmetic = {99, 888, 7};
        const auto weather =
            start_weather(before.weather.baseline(), before.weather.water(), before.weather.flow(), model, weather_hard_limits).value();
        const auto other = start_regional(weather, before.soil, regional_hard_limits).value();
        const auto x = advance_regional(other, 4, regional_hard_limits).value(),
                   y = advance_regional(
                           resume_regional(cosmetic, cosmetic.weather.baseline(), regional_hard_limits).value(), 4, regional_hard_limits
                   )
                           .value();
        CHECK(C::same(x.state, y.state));
        CHECK(x.report.transfers == y.report.transfers);
    }
    void invalid() {
        const auto s = C::literal();
        const auto held = capture_regional(s, regional_hard_limits).value();
        for (int k = 0; k < 3; ++k) {
            auto cp = held;
            if (k == 0)
                cp.version.schema = 2;
            if (k == 1)
                cp.version.soil_model = 2;
            if (k == 2)
                cp.version.numerics = 2;
            const auto r = resume_regional(cp, s.weather().baseline(), regional_hard_limits);
            CHECK(!r && r.error().kind == FailureKind::unsupported && r.error().reason == RegionalReason::version);
        }
        for (int k = 0; k < 7; ++k) {
            auto cp = held;
            if (k == 0)
                cp.soil.clear();
            if (k == 1)
                cp.soil[0].water_mm = -1;
            if (k == 2)
                cp.soil[0].capacity_mm = 1001;
            if (k == 3)
                cp.soil[0].infiltration_fraction = 1.1;
            if (k == 4)
                cp.soil[0].evaporation_mm_per_second = .2;
            if (k == 5)
                cp.soil[0].water_mm = std::numeric_limits<double>::quiet_NaN();
            if (k == 6)
                cp.soil[0].capacity_mm = std::numeric_limits<double>::infinity();
            CHECK(!resume_regional(cp, s.weather().baseline(), regional_hard_limits));
        }
        auto c = ProcGenMoistureChecks::context(100, 100, 1);
        c.frame.dx_metres = 200;
        const auto frame = ProcGenMoistureChecks::baseline(1, 1, std::array<Height, 1>{0}, std::array<ClimateSurface, 1>{}, c);
        CHECK(!resume_regional(held, frame, regional_hard_limits));
        c = ProcGenMoistureChecks::context(100, 100, 1);
        const auto height = ProcGenMoistureChecks::baseline(1, 1, std::array<Height, 1>{1}, std::array<ClimateSurface, 1>{}, c);
        CHECK(!resume_regional(held, height, regional_hard_limits));
        c.expected_revision = {2, 1};
        const auto revision = ProcGenMoistureChecks::baseline(1, 1, std::array<Height, 1>{0}, std::array<ClimateSurface, 1>{}, c);
        CHECK(!resume_regional(held, revision, regional_hard_limits));
        auto w = window(s, 0, 0, 1, 1);
        w.frame.north = RowNorth::increasing;
        CHECK(!observe_region(s, w, regional_hard_limits));
        w = window(s, 0, 0, 0, 1);
        CHECK(!observe_region(s, w, regional_hard_limits));
        w = window(s, std::numeric_limits<Index>::max(), 0, 1, 1);
        const auto overflow = observe_region(s, w, regional_hard_limits);
        CHECK(!overflow && overflow.error().reason == RegionalReason::overflow);
        CHECK(!advance_regional(s, 0, regional_hard_limits));
        CHECK(!advance_regional(s, 11, regional_hard_limits));
        near(held.soil[0].water_mm, .25);
        CHECK(!start_regional(s.weather(), {}, regional_hard_limits));
        const auto maxbase = ProcGenMoistureChecks::baseline(
            1, 1, std::array<Height, 1>{0}, std::array<ClimateSurface, 1>{}, ProcGenMoistureChecks::context(100, 100, 1),
            std::numeric_limits<Index>::max()
        );
        const auto maxweather =
            start_weather(maxbase, std::array<MoistureCell, 1>{}, std::array<FlowCell, 1>{}, C::passive(), weather_hard_limits).value();
        const auto maxstate = start_regional(maxweather, std::array<SoilCell, 1>{}, regional_hard_limits).value();
        const auto clock = advance_regional(maxstate, 1, regional_hard_limits);
        CHECK(!clock && clock.error().reason == RegionalReason::overflow);
        std::array<ClimateSurface, 1> changed_surface{};
        changed_surface[0].replenished_water_fraction = 1;
        const auto changed =
            ProcGenMoistureChecks::baseline(1, 1, std::array<Height, 1>{0}, changed_surface, ProcGenMoistureChecks::context(100, 100, 1));
        CHECK(!resume_regional(held, changed, regional_hard_limits));
        changed_surface[0] = {};
        changed_surface[0].roughness_metres = 1;
        const auto rough =
            ProcGenMoistureChecks::baseline(1, 1, std::array<Height, 1>{0}, changed_surface, ProcGenMoistureChecks::context(100, 100, 1));
        CHECK(!resume_regional(held, rough, regional_hard_limits));
    }
    void limits() {
        const auto s = flat(2, 2, 10);
        const auto r = regional_requirements(s.weather()).value();
        const auto cp = capture_regional(s, regional_hard_limits).value();
        auto b = regional_hard_limits;
        b.cells = 4;
        b.field_values = r.state_fields;
        b.storage_bytes = r.state_storage_bytes;
        b.work = r.copy_work;
        b.snapshots = 1;
        CHECK(start_regional(s.weather(), s.soil(), b));
        CHECK(capture_regional(s, b));
        CHECK(resume_regional(cp, s.weather().baseline(), b));
        for (int k = 0; k < 5; ++k) {
            auto x = b;
            if (k == 0)
                --x.cells;
            if (k == 1)
                --x.field_values;
            if (k == 2)
                --x.storage_bytes;
            if (k == 3)
                --x.work;
            if (k == 4)
                --x.snapshots;
            CHECK(!capture_regional(s, x));
            CHECK(!resume_regional(cp, s.weather().baseline(), x));
            if (k < 4)
                CHECK(!start_regional(s.weather(), s.soil(), x));
        }
        const auto out = advance_regional(s, 2, regional_hard_limits).value();
        b = regional_hard_limits;
        b.cells = 4;
        b.field_values = out.state.usage().field_values;
        b.storage_bytes = out.state.usage().storage_bytes;
        b.work = out.state.usage().work;
        b.ticks = 2;
        b.substeps_per_tick = 7;
        CHECK(advance_regional(s, 2, b));
        for (int k = 0; k < 6; ++k) {
            auto x = b;
            if (k == 0)
                --x.cells;
            if (k == 1)
                --x.field_values;
            if (k == 2)
                --x.storage_bytes;
            if (k == 3)
                --x.work;
            if (k == 4)
                --x.ticks;
            if (k == 5)
                --x.substeps_per_tick;
            CHECK(!advance_regional(s, 2, x));
        }
        const auto window1 = window(s, 0, 0, 1, 1);
        const auto observation = observe_region(s, window1, regional_hard_limits).value();
        b = regional_hard_limits;
        b.cells = 1;
        b.field_values = observation.usage.field_values;
        b.storage_bytes = observation.usage.storage_bytes;
        b.work = observation.usage.work;
        CHECK(observe_region(s, window1, b));
        for (int k = 0; k < 4; ++k) {
            auto x = b;
            if (k == 0)
                --x.cells;
            if (k == 1)
                --x.field_values;
            if (k == 2)
                --x.storage_bytes;
            if (k == 3)
                --x.work;
            CHECK(!observe_region(s, window1, x));
        }
    }
    void failure() {
        const auto s = flat(2, 2, 10);
        const auto out = advance_regional(s, 2, regional_hard_limits).value();
        auto cap = regional_hard_limits;
        cap.work = out.state.usage().work - 1;
        const auto bad = advance_regional(s, 2, cap);
        CHECK(!bad && bad.error().reason == RegionalReason::work);
        CHECK(bad.error().work > regional_requirements(s.weather())->copy_work);
        CHECK(s.weather().tick() == 0);
        const auto b = C::baseline();
        auto model = C::passive();
        std::array<MoistureCell, 1> q{};
        q[0].vapour_mm = 1e12 - .15;
        const auto weather = start_weather(b, q, std::array<FlowCell, 1>{}, model, weather_hard_limits).value();
        const auto unsafe = start_regional(weather, std::array<SoilCell, 1>{{{1, 1, 1, .1}}}, regional_hard_limits).value();
        const auto image = capture_regional(unsafe, regional_hard_limits).value();
        CHECK(advance_regional(unsafe, 1, regional_hard_limits));
        const auto numeric = advance_regional(unsafe, 2, regional_hard_limits);
        CHECK(!numeric && numeric.error().reason == RegionalReason::numeric);
        CHECK(unsafe.soil()[0].water_mm == 1);
        CHECK(unsafe.weather().water()[0] == image.weather.water()[0]);
    }
    void ownership() {
        auto make = []() {
            const auto s = C::literal();
            return capture_regional(s, regional_hard_limits).value();
        };
        const auto image = make();
        const auto b = image.weather.baseline();
        auto s = resume_regional(image, b, regional_hard_limits).value();
        auto copy = s;
        CHECK(s.soil().data() != image.soil.data());
        CHECK(s.weather().water().data() != image.weather.water().data());
        CHECK(copy.soil().data() != s.soil().data());
        const auto newer = advance_regional(copy, 3, regional_hard_limits).value();
        CHECK(C::same(s, copy));
        near(image.soil[0].water_mm, .25);
        const auto saved = newer;
        auto cap = regional_hard_limits;
        cap.storage_bytes = 0;
        CHECK(!advance_regional(newer.state, 1, cap));
        CHECK(C::same(saved.state, newer.state));
        CHECK(saved.report.transfers.data() != newer.report.transfers.data());
        bool allocation = false;
        RegionalAllocation::denied = true;
        try {
            static_cast<void>(capture_regional(s, regional_hard_limits));
        } catch (const std::bad_alloc&) {
            allocation = true;
        }
        RegionalAllocation::denied = false;
        CHECK(allocation);
        CHECK(C::same(s, copy));
        auto impossible = regional_hard_limits;
        impossible.field_values = 0;
        RegionalAllocation::denied = true;
        const auto early = capture_regional(s, impossible);
        RegionalAllocation::denied = false;
        CHECK(!early && early.error().reason == RegionalReason::fields);
    }
    void host() {
        using ProcGenRegionalHost::Application;
        Application app(C::literal());
        const auto pre = app.save(regional_hard_limits);
        auto rain = advance_regional(app.state(), 1, regional_hard_limits).value();
        const auto report = rain.report;
        const auto duplicate = rain;
        app.adopt(std::move(rain));
        CHECK(app.marker().wet_alerts == 1);
        const auto after = app.save(regional_hard_limits);
        auto restored = Application::restore(after, app.state().weather().baseline(), regional_hard_limits);
        CHECK(C::same(app.state(), restored.state()));
        CHECK(restored.ignore_old(report));
        CHECK(restored.ignore_old(report));
        CHECK(restored.marker().wet_alerts == 1);
        bool rejected = false;
        try {
            restored.adopt(duplicate);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        CHECK(rejected);
        CHECK(restored.marker().wet_alerts == 1);
        CHECK(C::same(app.state(), restored.state()));

        auto observation = observe_region(restored.state(), window(restored.state(), 0, 0, 1, 1), regional_hard_limits).value();
        CHECK(observe_region(restored.state(), observation.window, regional_hard_limits)->cells == observation.cells);
        CHECK(restored.marker().wet_alerts == 1);
        auto bad = after;
        bad.marker.last_applied_tick = 0;
        bool threw = false;
        try {
            static_cast<void>(Application::restore(bad, app.state().weather().baseline(), regional_hard_limits));
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        CHECK(threw);
        CHECK(pre.marker.wet_alerts == 0);
        CHECK(pre.checkpoint.weather.tick() == 0);
    }
}
int main(int argc, char** argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("one regional case required");
        const std::string n = argv[1];
        if (n == "regional_soil")
            soil();
        else if (n == "regional_drying")
            drying();
        else if (n == "regional_supply")
            supply();
        else if (n == "regional_resume")
            resume();
        else if (n == "regional_history")
            history();
        else if (n == "regional_regions")
            regions();
        else if (n == "regional_signals")
            signals();
        else if (n == "regional_generated")
            generated();
        else if (n == "regional_invalid")
            invalid();
        else if (n == "regional_limits")
            limits();
        else if (n == "regional_failure")
            failure();
        else if (n == "regional_ownership")
            ownership();
        else if (n == "regional_host")
            host();
        else
            throw std::runtime_error("unknown regional case");
        std::cout << "PASS " << n << ": " << assertions << " assertions\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL regional: " << e.what() << '\n';
        return 1;
    }
}
