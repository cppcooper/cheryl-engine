#include "background-weather.h"
#include "background-fixture.h"
#include <core/logging/process-lifetime.h>
#include <array>
#include <iostream>
#include <iomanip>
#include <stdexcept>

namespace {
    using namespace ProcGen;
    using BW=BackgroundWeather;
    void require(bool ok,const char* text) { if (!ok) throw std::runtime_error(text); }
    double elapsed(BW::Clock::time_point begin) { return std::chrono::duration<double,std::milli>(BW::Clock::now()-begin).count(); }
    template<class F> auto retry(F call) {
        auto result=call();
        for (int n=0;n<10000 && !result && result.error()==BW::Code::busy;++n) { std::this_thread::yield(); result=call(); }
        return result;
    }
    void run(Index width,Index height,Index seconds) {
        BW weather(ProcGenBackgroundFixture::make(width,height,seconds));
        require(weather.wait_idle(std::chrono::seconds(5)),"initial endpoint watchdog");
        BW::Simulation simulation;
        auto first=retry([&] { return weather.pin_latest(); }); require(first.has_value(),"initial pin");
        auto pin=std::move(*first);
        require(retry([&] { return weather.adopt(pin,simulation); }).has_value(),"initial adoption");
        for (Index i=0;i<4;++i) {
            const auto old=pin.token();
            require(retry([&] { return weather.request(old); }).has_value(),"bounded background request");
            const auto before=CE::Memory::Detail::thread_allocations;
            const auto copies_before=CopyMetrics::baseline_bytes+CopyMetrics::weather_bytes+CopyMetrics::soil_bytes;
            const auto reading=BW::Clock::now();
            double consumed=0;
            for (std::size_t n=0;n<32;++n) {
                const auto sample=weather.sample(pin,n%(static_cast<std::size_t>(width*height)),old.to_tick+2).value();
                require(sample.age_ticks==2 && sample.token==old,"late pinned sample age");
                consumed+=sample.surface_kelvin+sample.soil_mm;
            }
            const auto late_read=elapsed(reading);
            require(CE::Memory::Detail::thread_allocations==before,"foreground late reads allocate/free no Engine data");
            require(CopyMetrics::baseline_bytes+CopyMetrics::weather_bytes+CopyMetrics::soil_bytes==copies_before,"late reads copy no grid");
            require(weather.wait_idle(std::chrono::seconds(5)),"background completion watchdog"); // control/harness wait, outside foreground timers
            const auto counts=CE::Memory::Detail::thread_allocations;
            const auto pinning=BW::Clock::now();
            auto next=retry([&] { return weather.pin_latest(); }); require(next.has_value(),"completed endpoint pin");
            const auto pin_ms=elapsed(pinning);
            const auto sample_start=BW::Clock::now();
            const auto sample=weather.sample(*next,0,next->token().to_tick+1).value();
            const auto read_ms=elapsed(sample_start);
            const auto adopting=BW::Clock::now();
            auto adopted=retry([&] { return weather.adopt(*next,simulation); }); require(adopted.has_value(),"contiguous interval adoption");
            const auto adopt_ms=elapsed(adopting);
            const auto retiring=BW::Clock::now();
            BW::Code code=BW::Code::busy;
            for (int n=0;n<10000 && code==BW::Code::busy;++n) { code=weather.retire(old); if (code==BW::Code::busy) std::this_thread::yield(); }
            require(code==BW::Code::ok,"explicit old-endpoint retirement");
            pin.reset(); pin=std::move(*next);
            const auto retire_ms=elapsed(retiring);
            require(CE::Memory::Detail::thread_allocations==counts,"foreground pin/read/adopt/retire performs no Engine allocation/free");
            require(CopyMetrics::baseline_bytes+CopyMetrics::weather_bytes+CopyMetrics::soil_bytes==copies_before,"foreground copies no grid");
            require(simulation.intervals==i+1 && sample.age_ticks==1 && consumed>0,"sampled and adopted real state");
            std::cout<<"{\"kind\":\"foreground\",\"grid\":["<<width<<','<<height<<"],\"interval\":"<<i
                <<",\"model_tick\":"<<simulation.cursor.to_tick<<",\"sample_age_ticks\":"<<sample.age_ticks
                <<",\"publication_age_ms\":"<<sample.publication_age_ms<<",\"late_read_32_ms\":"<<late_read
                <<",\"pin_ms\":"<<pin_ms<<",\"read_ms\":"<<read_ms<<",\"adopt_ms\":"<<adopt_ms<<",\"retire_release_ms\":"<<retire_ms
                <<",\"engine_foreground_churn\":0,\"copied_grid_bytes\":0}\n";
        }
        pin.reset(); const auto closing=BW::Clock::now(); require(weather.close()==BW::Code::ok,"shutdown");
        const auto shutdown_ms=elapsed(closing); const auto status=weather.status();
        require(status.done && status.occupied==0 && status.producer==status.last_reclaimer && status.producer!=std::this_thread::get_id(),"producer-side reclamation and join");
        for (std::size_t n=0;n<status.timing_count;++n) {
            const auto& t=status.timings[n];
            std::cout<<"{\"kind\":\"producer\",\"grid\":["<<width<<','<<height<<"],\"sequence\":"<<t.token.sequence
                <<",\"solver_ms\":"<<t.solver_ms<<",\"reclaim_ms\":"<<t.reclaim_ms<<",\"work\":"<<t.work
                <<",\"engine_checkouts\":"<<t.allocations.checkouts<<",\"backing_calls\":"<<t.allocations.backing_calls
                <<",\"backing_frees\":"<<t.allocations.backing_frees<<",\"metadata_calls\":"<<t.allocations.metadata_calls
                <<",\"metadata_frees\":"<<t.allocations.metadata_frees<<",\"control_calls\":"<<t.allocations.control_calls
                <<",\"control_frees\":"<<t.allocations.control_frees<<"}\n";
        }
        std::cout<<"{\"kind\":\"shutdown\",\"grid\":["<<width<<','<<height<<"],\"ms\":"<<shutdown_ms<<",\"all_endpoints_drained\":true}\n";
    }
}
int main() {
    try { CE::Logging::ProcessLifetime logging; std::cout<<std::setprecision(17); run(32,32,60); run(64,48,120); }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
