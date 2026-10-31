#include "background-weather.h"
#include "background-fixture.h"
#include "allocation-denial.h"
#include <core/logging/process-lifetime.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    using namespace ProcGen;
    using BW=BackgroundWeather;
    using Code=BW::Code;
    constexpr auto watchdog=std::chrono::seconds(5);
    std::size_t assertions=0;
    template<class T> void check(const T& value,const char* message) {
        ++assertions; if (!static_cast<bool>(value)) throw std::runtime_error(message);
    }
    bool busy(Code c) { return c==Code::busy; }
    template<class T> bool busy(const std::expected<T,Code>& c) { return !c && c.error()==Code::busy; }
    template<class F> auto retry(F f) {
        auto result=f();
        for (int i=0;i<10000 && busy(result);++i) { std::this_thread::yield(); result=f(); }
        return result;
    }
    BW::Pin latest(BW& weather) {
        auto p=retry([&] { return weather.pin_latest(); }); check(p,"pin latest"); return std::move(*p);
    }
    void idle(BW& weather) { check(weather.wait_idle(watchdog),"bounded completion watchdog"); }
    void adopt(BW& weather,const BW::Pin& p,BW::Simulation& sim) {
        auto result=retry([&] { return weather.adopt(p,sim); }); check(result,"adopt");
    }
    void request(BW& weather,BW::Token token) {
        auto result=retry([&] { return weather.request(token); }); check(result,"request"); idle(weather);
        check(weather.status().last==Code::ok,"successful producer");
    }
    struct Gate {
        std::mutex mutex;
        std::condition_variable cv;
        bool entered=false, opened=false;
        std::thread::id thread;
        static void enter(void* pointer,const BW::Token&) noexcept {
            auto& g=*static_cast<Gate*>(pointer);
            std::unique_lock lock(g.mutex); g.entered=true; g.thread=std::this_thread::get_id(); g.cv.notify_all();
            g.cv.wait(lock,[&] { return g.opened; });
        }
        bool wait() {
            std::unique_lock lock(mutex); return cv.wait_for(lock,watchdog,[&] { return entered; });
        }
        void open() noexcept { std::lock_guard lock(mutex); opened=true; cv.notify_all(); }
    };
    struct OpenOnExit { Gate& gate; ~OpenOnExit() { gate.open(); } };
    void late_and_replay(Index width,Index height,Index seconds) {
        Gate gate;
        BW weather(ProcGenBackgroundFixture::make(width,height,seconds),{&gate,Gate::enter,nullptr,nullptr});
        OpenOnExit release{gate};
        idle(weather);
        BW::Simulation sim;
        auto old=latest(weather); adopt(weather,old,sim);
        auto serial=old.state();
        RuntimeAllowance budget;
        WeatherWorkspace scratch;
        auto serial_result=advance_runtime(serial,1,budget,scratch);
        if (!serial_result) throw std::runtime_error("serial advance reason="+std::to_string(static_cast<int>(serial_result.error().reason)));
        const auto expected=std::move(*serial_result);
        const auto pending=retry([&] { return weather.request(old.token()); }); check(pending,"late request accepted");
        check(gate.wait(),"producer paused at deterministic gate");
        check(gate.thread!=std::this_thread::get_id(),"actual background producer");
        const auto counts=CE::Memory::Detail::thread_allocations;
        const auto copies=CopyMetrics::baseline_bytes+CopyMetrics::weather_bytes+CopyMetrics::soil_bytes;
        auto still=latest(weather);
        const auto sample=weather.sample(still,0,old.token().to_tick+7).value();
        check(sample.token==old.token() && sample.age_ticks==7 && sample.publication_age_ms>=0,"late reads old complete endpoint and exact age");
        check(sim.intervals==0,"sampling applies no rain");
        auto duplicate=retry([&] { return weather.adopt(still,sim); }); check(!duplicate && duplicate.error()==Code::duplicate,"duplicate base adoption");
        check(CE::Memory::Detail::thread_allocations==counts,"foreground pin/read/adopt rejection has no Engine allocation or free");
        check(copies==CopyMetrics::baseline_bytes+CopyMetrics::weather_bytes+CopyMetrics::soil_bytes,"foreground copies no grid");
        still.reset(); gate.open(); idle(weather);
        auto next=latest(weather);
        check(ProcGenRegionalChecks::same(next.state(),expected.state),"background equals real serial solver");
        static_cast<void>(ProcGenRegionalChecks::inspect(serial,expected));
        adopt(weather,next,sim);
        check(sim.intervals==1 && sim.cursor.to_tick==old.token().to_tick+1,"contiguous interval adopted once");
        check(std::abs(sim.infiltration_m3-expected.report.water.infiltration_m3)<1e-9,"authoritative interval consequence once");
        auto again=retry([&] { return weather.adopt(next,sim); }); check(!again && again.error()==Code::duplicate,"duplicate interval refuses");
        auto foreign=sim; ++foreign.cursor.stream;
        auto wrong=retry([&] { return weather.adopt(next,foreign); }); check(!wrong && wrong.error()==Code::foreign,"foreign simulation adoption refuses");
        check(retry([&] { return weather.retire(old.token()); })==Code::ok,"explicit retirement");
        check(weather.sample(old,0,old.token().to_tick).has_value(),"retired held snapshot readable");
        old.reset(); next.reset(); check(weather.close()==Code::ok,"shutdown joins");
        const auto status=weather.status();
        check(status.done && status.occupied==0 && status.last_reclaimer==status.producer,"producer drains every endpoint");
    }
    void slots_and_order() {
        Gate reclaim_gate;
        BW weather(ProcGenBackgroundFixture::make(8,6),{&reclaim_gate,nullptr,nullptr,Gate::enter});
        OpenOnExit release{reclaim_gate}; idle(weather);
        BW::Simulation sim; auto p0=latest(weather); adopt(weather,p0,sim);
        auto zero=retry([&] { return weather.request(p0.token(),0); });
        check(!zero && zero.error()==Code::profile,"zero tick request refuses before work");
        auto multiple=retry([&] { return weather.request(p0.token(),2); });
        check(!multiple && multiple.error()==Code::profile,"multi tick request refuses before work");
        request(weather,p0.token()); auto p1=latest(weather);
        request(weather,p1.token()); auto p2=latest(weather);
        auto out_of_order=retry([&] { return weather.adopt(p2,sim); });
        check(!out_of_order && out_of_order.error()==Code::order && sim.intervals==0,"out-of-order adoption preserves marker");
        auto foreign=p2.token(); ++foreign.stream;
        auto refused=retry([&] { return weather.pin(foreign); }); check(!refused && refused.error()==Code::foreign,"foreign token refuses");
        auto extra=latest(weather);
        auto fifth=retry([&] { return weather.pin_latest(); }); check(!fifth && fifth.error()==Code::pins,"fixed pin limit"); extra.reset();
        adopt(weather,p1,sim); check(retry([&] { return weather.retire(p0.token()); })==Code::ok,"retire first held endpoint");
        check(retry([&] { return weather.retire(p0.token()); })==Code::duplicate,"duplicate retirement refuses");
        adopt(weather,p2,sim); check(retry([&] { return weather.retire(p1.token()); })==Code::ok,"retire second held endpoint");
        check(retry([&] { return weather.retire(p2.token()); })==Code::current,"cannot retire current/head");
        const auto counts=CE::Memory::Detail::thread_allocations;
        auto full=retry([&] { return weather.request(p2.token()); }); check(!full && full.error()==Code::full,"all endpoint slots held: bounded refusal");
        check(CE::Memory::Detail::thread_allocations==counts,"full refusal has no Engine churn");
        const auto token0=p0.token(); const auto reclaimed=weather.status().reclaimed;
        p0.reset(); check(reclaim_gate.wait(),"producer paused before heavy reclamation");
        const auto foreground=CE::Memory::Detail::thread_allocations;
        auto during_reclaim=latest(weather);
        check(weather.sample(during_reclaim,0,p2.token().to_tick).has_value(),"foreground remains readable during reclamation");
        auto reclaim_full=retry([&] { return weather.request(p2.token()); });
        check(!reclaim_full && reclaim_full.error()==Code::full,"reclaiming slot stays unavailable");
        during_reclaim.reset();
        check(CE::Memory::Detail::thread_allocations==foreground,"foreground does no reclaimer work");
        reclaim_gate.open();
        check(weather.wait_reclaimed(reclaimed+1,watchdog),"producer reclaims released retired endpoint");
        request(weather,p2.token()); auto p3=latest(weather);
        check(p3.token().slot==token0.slot && p3.token().generation!=token0.generation,"slot generation changes on reuse");
        auto stale=retry([&] { return weather.pin(token0); }); check(!stale && stale.error()==Code::stale,"stale slot token refuses");
        check(weather.sample(p1,0,p3.token().to_tick)->token==p1.token(),"older pinned endpoint was not overwritten");
        adopt(weather,p3,sim); check(sim.intervals==3,"three ordered intervals");
        check(retry([&] { return weather.retire(p2.token()); })==Code::ok,"retire prior current");
        auto replacement=ProcGenBackgroundFixture::make(8,6);
        check(retry([&] { return weather.restore(std::move(replacement)); })==Code::ok,"restore with all three slots held defers base publication");
        check(weather.status().occupied==3,"restore never grows held slot set");
        check(!weather.pin_latest(),"pending full restore has no new-epoch endpoint yet");
        check(weather.sample(p3,0,p3.token().to_tick)->old_epoch,"full restore preserves old held state");
        p1.reset(); idle(weather); auto restored=latest(weather); adopt(weather,restored,sim);
        check(sim.intervals==0 && restored.token().epoch!=p3.token().epoch,"one released slot allows new base adoption");
        p2.reset(); p3.reset(); restored.reset(); check(weather.close()==Code::ok,"full-slot shutdown");
    }
    void restore_and_stop() {
        Gate gate;
        BW weather(ProcGenBackgroundFixture::make(8,6),{&gate,Gate::enter,nullptr,nullptr});
        OpenOnExit release{gate}; idle(weather);
        BW::Simulation sim; auto old=latest(weather); adopt(weather,old,sim);
        auto pending=retry([&] { return weather.request(old.token()); }); check(pending,"restore in-flight request");
        check(gate.wait(),"restore gate");
        auto replacement=ProcGenBackgroundFixture::make(16,8,120);
        const auto expected_tick=replacement.weather().tick();
        check(retry([&] { return weather.restore(std::move(replacement)); })==Code::ok,"restore starts a new epoch");
        auto second=ProcGenBackgroundFixture::make(8,6);
        const auto* retained=second.weather().water().data();
        check(retry([&] { return weather.restore(std::move(second)); })==Code::busy,"only one pending restore");
        check(second.weather().water().data()==retained,"refused restore retains caller ownership");
        check(weather.sample(old,0,old.token().to_tick)->old_epoch,"old pinned epoch remains readable and tagged");
        auto rejected=retry([&] { return weather.adopt(old,sim); }); check(!rejected && rejected.error()==Code::stale,"restore rejects old adoption token");
        gate.open(); idle(weather);
        auto base=latest(weather);
        check(base.token().epoch!=old.token().epoch && base.token().to_tick==expected_tick,"new epoch base published");
        check(weather.status().discarded==1,"obsolete completed candidate discarded");
        adopt(weather,base,sim); check(sim.intervals==0,"restore resets timeline marker without interval replay");
        check(weather.sample(old,0,old.token().to_tick)->token==old.token(),"old snapshot survives new epoch publication");
        old.reset();
        check(weather.close()==Code::pins,"shutdown refuses held pins without joining");
        check(!weather.pin_latest() && weather.sample(base,0,base.token().to_tick).has_value(),"closing refuses new pins but keeps held state readable");
        base.reset(); check(weather.close()==Code::ok,"release then shutdown joins");
        check(weather.status().occupied==0 && weather.status().done,"shutdown drains owners on producer");
    }
    void refuse_and_exception() {
        {
            BW weather(ProcGenBackgroundFixture::make(4,4,120,1,true)); idle(weather);
            auto p=latest(weather); BW::Simulation sim; adopt(weather,p,sim);
            const auto token=p.token();
            auto ticket=retry([&] { return weather.request(token); }); check(ticket,"solver refusal request admitted"); idle(weather);
            check(weather.status().last==Code::solver && weather.status().failure.reason==RegionalReason::substeps,"real substep refusal published as error");
            check(latest(weather).token()==token && sim.intervals==0,"solver refusal preserves live state");
            p.reset(); check(weather.close()==Code::ok,"error shutdown");
        }
        {
            BW::Hooks hooks;
            hooks.before_compute=[](void*,const BW::Token&) noexcept { ProcGenTestAllocation::denied=true; };
            hooks.after_compute=[](void*,const BW::Token&) noexcept { ProcGenTestAllocation::denied=false; };
            BW weather(ProcGenBackgroundFixture::make(8,6),hooks); idle(weather);
            auto p=latest(weather); const auto token=p.token();
            auto ticket=retry([&] { return weather.request(token); }); check(ticket,"exception request"); idle(weather);
            const auto state=weather.status();
            check(state.last==Code::exception && state.updates_left==0 && state.exception_text[0],"worker allocation exception fails batch closed");
            check(latest(weather).token()==token,"exception leaves prior publication intact");
            auto budget=retry([&] { return weather.request(token); }); check(!budget && budget.error()==Code::budget,"poisoned allowance refuses further work");
            p.reset(); check(weather.close()==Code::ok,"exception shutdown");
        }
        {
            auto invalid=ProcGenBackgroundFixture::make(65,1);
            const auto* cells=invalid.weather().water().data(); bool refused=false;
            try { BW weather(std::move(invalid)); } catch(const std::invalid_argument&) { refused=true; }
            check(refused && invalid.weather().water().data()==cells,"invalid initial profile refuses before ownership transfer/thread");
        }
    }
    void shutdown_inflight() {
        Gate gate;
        BW weather(ProcGenBackgroundFixture::make(8,6),{&gate,Gate::enter,nullptr,nullptr});
        OpenOnExit release{gate}; idle(weather);
        auto held=latest(weather); const auto token=held.token();
        check(retry([&] { return weather.request(token); }).has_value(),"shutdown request");
        check(gate.wait(),"shutdown producer in-flight gate");
        check(weather.close()==Code::pins,"in-flight shutdown marks closing and preserves permitted pin");
        check(weather.sample(held,0,token.to_tick).has_value(),"closing old snapshot readable");
        auto refused=retry([&] { return weather.request(token); });
        check(!refused && refused.error()==Code::closed,"closing refuses more producer work");
        gate.open(); held.reset(); check(weather.close()==Code::ok,"in-flight work joins and drains");
        const auto status=weather.status();
        check(status.done && status.occupied==0 && status.published==1 && status.discarded==1,"shutdown never publishes obsolete candidate");
    }
}
int main() {
    try {
        CE::Logging::ProcessLifetime logging;
        std::cout<<"late/replay 32x32 and64x48\n"<<std::flush; late_and_replay(32,32,60); late_and_replay(64,48,120);
        std::cout<<"slots/order\n"<<std::flush; slots_and_order();
        std::cout<<"restore/stop\n"<<std::flush; restore_and_stop();
        std::cout<<"refusal/exception\n"<<std::flush; refuse_and_exception();
        std::cout<<"shutdown in-flight\n"<<std::flush; shutdown_inflight();
        std::cout<<"Background weather: "<<assertions<<" assertions, deterministic late/full/order/restore/error/shutdown checks pass\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
