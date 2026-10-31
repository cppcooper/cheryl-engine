#include "background-weather.h"
#include <core/resources/objects/object-reservation.hpp>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ProcGen {
    namespace {
        Index stream_id() {
            static std::atomic<Index> next{1};
            auto n = next.load();
            while (n != std::numeric_limits<Index>::max())
                if (next.compare_exchange_weak(n, n + 1)) return n;
            throw std::overflow_error("weather stream identity exhausted");
        }
        std::shared_ptr<WeatherWorkspace> workspace() {
            CE::Memory::Objects::Pool<WeatherWorkspace> pool;
            CE::Memory::Allocators::ObjectPoolAllocator<WeatherWorkspace> allocator(pool.release_context());
            CE::Memory::Objects::Reservation<WeatherWorkspace> slots(1, allocator);
            return slots.emplace(0);
        }
        double ms(BackgroundWeather::Clock::time_point a, BackgroundWeather::Clock::time_point b) {
            return std::chrono::duration<double, std::milli>(b - a).count();
        }
        CE::Memory::Detail::ThreadAllocationCounts difference(
            CE::Memory::Detail::ThreadAllocationCounts a, CE::Memory::Detail::ThreadAllocationCounts b
        ) {
            return {a.backing_calls-b.backing_calls,a.backing_frees-b.backing_frees,
                    a.metadata_calls-b.metadata_calls,a.metadata_frees-b.metadata_frees,
                    a.control_calls-b.control_calls,a.control_frees-b.control_frees,a.checkouts-b.checkouts};
        }
    }
    bool BackgroundWeather::profile(const RegionalState& state) {
        const auto& d = state.weather().baseline().descriptor();
        return d.domain.dimensions() == 2 && d.domain.extents()[0] <= 64 && d.domain.extents()[1] <= 48 &&
               d.domain.cells() <= 3072 && d.context.calendar.seconds_per_tick <= 120 && state.weather().model().forcing.count <= 2;
    }
    BackgroundWeather::BackgroundWeather(RegionalState&& initial) : BackgroundWeather(std::move(initial), Hooks{}) {}
    BackgroundWeather::BackgroundWeather(RegionalState&& initial, Hooks hooks) : stream_(stream_id()), hooks_(hooks) {
        if (!profile(initial)) throw std::invalid_argument("background weather initial profile refused");
        restore_.emplace(std::move(initial));
        thread_ = std::thread([this] { run(); });
    }
    BackgroundWeather::~BackgroundWeather() {
        if (close() == Code::pins) std::terminate(); // borrowing lifetime contract, never detach a producer
    }
    BackgroundWeather::Pin::Pin(Pin&& other) noexcept
    : owner_(std::exchange(other.owner_, nullptr)), frame_(std::exchange(other.frame_, nullptr)), token_(other.token_) {}
    BackgroundWeather::Pin& BackgroundWeather::Pin::operator=(Pin&& other) noexcept {
        if (this != &other) {
            reset(); owner_ = std::exchange(other.owner_, nullptr); frame_ = std::exchange(other.frame_, nullptr); token_ = other.token_;
        }
        return *this;
    }
    void BackgroundWeather::Pin::reset() noexcept {
        if (owner_) owner_->release(token_.slot);
        owner_ = nullptr; frame_ = nullptr;
    }
    const RegionalState& BackgroundWeather::Pin::state() const {
        if (!owner_) throw std::logic_error("released weather pin");
        return frame_->state;
    }
    void BackgroundWeather::release(std::size_t slot) noexcept {
        // Pair the predicate change with cv_ waits. Notification without this
        // short metadata lock could lose the final release before a wait starts.
        std::lock_guard lock(mutex_);
        slots_[slot].pins.fetch_sub(1, std::memory_order_acq_rel);
        pins_.fetch_sub(1, std::memory_order_acq_rel);
        cv_.notify_all();
    }
    BackgroundWeather::Code BackgroundWeather::validate(const Token& t) const {
        if (t.stream != stream_) return Code::foreign;
        if (t.epoch != epoch_.load() || t.slot >= slot_count) return Code::stale;
        const auto& s = slots_[t.slot];
        if (s.token != t || (s.phase != Phase::published && s.phase != Phase::retired)) return Code::stale;
        return Code::ok;
    }
    std::size_t BackgroundWeather::free_slot() const {
        for (std::size_t i = 0; i < slot_count; ++i)
            if (slots_[i].phase == Phase::free && slots_[i].generation != std::numeric_limits<Index>::max()) return i;
        return slot_count;
    }
    bool BackgroundWeather::reclaimable() const {
        for (const auto& s : slots_)
            if (s.phase == Phase::retired && s.pins.load(std::memory_order_acquire) == 0) return true;
        return false;
    }
    std::expected<BackgroundWeather::Pin, BackgroundWeather::Code> BackgroundWeather::pin_latest() {
        std::unique_lock lock(mutex_, std::try_to_lock);
        if (!lock.owns_lock()) return std::unexpected(Code::busy);
        if (closing_) return std::unexpected(Code::closed);
        if (!ready_) return std::unexpected(Code::busy);
        auto& s = slots_[head_];
        if (pins_.load() >= pin_limit) return std::unexpected(Code::pins);
        s.pins.fetch_add(1); pins_.fetch_add(1);
        return Pin(this, &*s.frame, s.token);
    }
    std::expected<BackgroundWeather::Pin, BackgroundWeather::Code> BackgroundWeather::pin(Token t) {
        std::unique_lock lock(mutex_, std::try_to_lock);
        if (!lock.owns_lock()) return std::unexpected(Code::busy);
        if (closing_) return std::unexpected(Code::closed);
        const auto code = validate(t);
        if (code != Code::ok) return std::unexpected(code);
        auto& s = slots_[t.slot];
        if (s.phase != Phase::published) return std::unexpected(Code::stale);
        if (pins_.load() >= pin_limit) return std::unexpected(Code::pins);
        s.pins.fetch_add(1); pins_.fetch_add(1);
        return Pin(this, &*s.frame, t);
    }
    std::expected<BackgroundWeather::Ticket, BackgroundWeather::Code> BackgroundWeather::request(Token t, Index ticks) {
        std::unique_lock lock(mutex_, std::try_to_lock);
        if (!lock.owns_lock()) return std::unexpected(Code::busy);
        if (closing_) return std::unexpected(Code::closed);
        const auto valid = validate(t);
        if (valid != Code::ok) return std::unexpected(valid);
        if (ticks != 1) return std::unexpected(Code::profile);
        if (busy_ || job_ || !ready_) return std::unexpected(Code::busy);
        if (t != slots_[head_].token) return std::unexpected(Code::order);
        if (!status_.updates_left || status_.work_left < runtime_update_work ||
            next_request_ == std::numeric_limits<Index>::max() || t.sequence == std::numeric_limits<Index>::max() ||
            t.to_tick == std::numeric_limits<Index>::max()) return std::unexpected(Code::budget);
        const auto index = free_slot();
        if (index == slot_count) return std::unexpected(Code::full);
        auto& s = slots_[index];
        if (s.generation == std::numeric_limits<Index>::max()) return std::unexpected(Code::budget);
        Token next{stream_, t.epoch, ++s.generation, ++next_request_, t.sequence + 1, t.to_tick, t.to_tick + 1, index};
        s.phase = Phase::building; s.token = next;
        job_ = Job{next, head_};
        cv_.notify_one();
        return Ticket{stream_, t.epoch, next.request};
    }
    std::expected<BackgroundWeather::Adoption, BackgroundWeather::Code>
    BackgroundWeather::adopt(const Pin& pin, Simulation& sim) {
        std::unique_lock lock(mutex_, std::try_to_lock);
        if (!lock.owns_lock()) return std::unexpected(Code::busy);
        if (closing_) return std::unexpected(Code::closed);
        if (pin.owner_ != this) return std::unexpected(Code::foreign);
        const auto valid = validate(pin.token_);
        if (valid != Code::ok) return std::unexpected(valid);
        const auto t = pin.token_;
        if (sim.cursor.stream && sim.cursor.stream != stream_) return std::unexpected(Code::foreign);
        if (sim.cursor != adopted_) return std::unexpected(Code::order);
        if (t == adopted_) return std::unexpected(Code::duplicate);
        Simulation next = sim;
        if (!pin.frame_->report) {
            if (adopted_.epoch == t.epoch || t.sequence != 0) return std::unexpected(Code::order);
            next.intervals = 0; next.infiltration_m3 = 0; next.overflow_m3 = 0;
        } else {
            if (adopted_.epoch != t.epoch || t.sequence != adopted_.sequence + 1 || t.from_tick != adopted_.to_tick ||
                t.to_tick != t.from_tick + 1) return std::unexpected(Code::order);
            if (sim.intervals == std::numeric_limits<Index>::max()) return std::unexpected(Code::budget);
            ++next.intervals;
            next.infiltration_m3 += pin.frame_->report->water.infiltration_m3;
            next.overflow_m3 += pin.frame_->report->water.overflow_export_m3;
            if (!std::isfinite(next.infiltration_m3) || !std::isfinite(next.overflow_m3)) return std::unexpected(Code::budget);
        }
        const auto old = adopted_;
        next.cursor = t;
        sim = next; adopted_ = t;
        return Adoption{old, next.intervals};
    }
    BackgroundWeather::Code BackgroundWeather::retire(Token t) {
        std::unique_lock lock(mutex_, std::try_to_lock);
        if (!lock.owns_lock()) return Code::busy;
        if (closing_) return Code::closed;
        const auto valid = validate(t);
        if (valid != Code::ok) return valid;
        if (t == adopted_ || t.slot == head_) return Code::current;
        if (t.sequence >= adopted_.sequence) return Code::order;
        auto& s = slots_[t.slot];
        if (s.phase == Phase::retired) return Code::duplicate;
        s.phase = Phase::retired;
        cv_.notify_one();
        return Code::ok;
    }
    std::expected<BackgroundWeather::Sample, BackgroundWeather::Code>
    BackgroundWeather::sample(const Pin& pin, std::size_t cell, Index now) const {
        if (pin.owner_ != this) return std::unexpected(Code::foreign);
        const auto& f = *pin.frame_;
        if (cell >= f.state.soil().size() || now < f.token.to_tick) return std::unexpected(Code::profile);
        const auto& wind = f.state.weather().flow()[cell];
        return Sample{f.token, now-f.token.to_tick, ms(f.published, Clock::now()), wind.surface_kelvin, wind.east_m_s,
                      wind.north_m_s, f.state.weather().water()[cell].vapour_mm, f.state.soil()[cell].water_mm,
                      f.token.epoch != epoch_.load()};
    }
    BackgroundWeather::Code BackgroundWeather::restore(RegionalState&& next) {
        if (!profile(next)) return Code::profile;
        std::unique_lock lock(mutex_, std::try_to_lock);
        if (!lock.owns_lock()) return Code::busy;
        if (closing_) return Code::closed;
        if (restore_ || !ready_) return Code::busy;
        if (epoch_.load() == std::numeric_limits<Index>::max()) return Code::budget;
        ++epoch_; ready_ = false;
        if (job_) { slots_[job_->token.slot].phase = Phase::free; job_.reset(); ++status_.discarded; }
        for (auto& s : slots_) if (s.phase == Phase::published) s.phase = Phase::retired;
        restore_.emplace(std::move(next));
        cv_.notify_one();
        return Code::ok;
    }
    BackgroundWeather::Code BackgroundWeather::close() {
        {
            std::lock_guard lock(mutex_);
            closing_ = true;
            if (job_) { slots_[job_->token.slot].phase = Phase::free; job_.reset(); ++status_.discarded; }
            for (auto& s : slots_) if (s.phase == Phase::published) s.phase = Phase::retired;
            cv_.notify_one();
            if (pins_.load()) return Code::pins;
        }
        if (thread_.joinable()) thread_.join();
        return Code::ok;
    }
    BackgroundWeather::Status BackgroundWeather::status() const {
        std::lock_guard lock(mutex_);
        auto result = status_;
        result.epoch = epoch_.load(); result.pins = pins_.load(); result.busy = busy_ || job_.has_value();
        result.closing = closing_; result.done = done_;
        for (const auto& s : slots_) if (s.phase != Phase::free) ++result.occupied;
        return result;
    }
    bool BackgroundWeather::wait_idle(std::chrono::milliseconds watchdog) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, watchdog, [&] { return done_ || (ready_ && !busy_ && !job_ && !restore_ && !reclaimable()); });
    }
    bool BackgroundWeather::wait_reclaimed(Index n, std::chrono::milliseconds watchdog) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, watchdog, [&] { return status_.reclaimed >= n || done_; });
    }
    void BackgroundWeather::timing(Timing value) {
        if (status_.timing_count < status_.timings.size()) status_.timings[status_.timing_count++] = value;
    }
    void BackgroundWeather::reclaim(std::size_t index) {
        const auto token = slots_[index].token;
        if (hooks_.before_reclaim) hooks_.before_reclaim(hooks_.context, token);
        const auto counts = CE::Memory::Detail::thread_allocations;
        const auto begin = Clock::now();
        slots_[index].frame.reset();
        const auto duration = ms(begin, Clock::now());
        std::lock_guard lock(mutex_);
        slots_[index].phase = Phase::free;
        ++status_.reclaimed; status_.last_reclaimer = std::this_thread::get_id();
        timing({token,0,duration,0,difference(CE::Memory::Detail::thread_allocations, counts)});
        cv_.notify_all();
    }
    void BackgroundWeather::run() noexcept {
        try {
            auto scratch = workspace();
            RuntimeAllowance allowance;
            std::unique_lock lock(mutex_);
            status_.producer = std::this_thread::get_id();
            for (;;) {
                std::size_t retired = slot_count;
                for (std::size_t i=0;i<slot_count;++i)
                    if (slots_[i].phase == Phase::retired && slots_[i].pins.load(std::memory_order_acquire)==0) { retired=i; break; }
                if (retired != slot_count) {
                    slots_[retired].phase = Phase::reclaiming;
                    lock.unlock(); reclaim(retired); lock.lock(); continue;
                }
                if (closing_) {
                    if (restore_) {
                        auto discarded = std::move(*restore_); restore_.reset();
                        lock.unlock(); { auto owned = std::move(discarded); static_cast<void>(owned); } lock.lock();
                    }
                    if (pins_.load()==0) {
                        lock.unlock(); scratch.reset(); lock.lock();
                        done_=true; cv_.notify_all(); return;
                    }
                } else if (restore_ && free_slot()!=slot_count) {
                    const auto index=free_slot(); auto& slot=slots_[index];
                    const auto tick=restore_->weather().tick();
                    Token token{stream_,epoch_.load(),++slot.generation,0,0,tick,tick,index};
                    slot.phase=Phase::building; slot.token=token; busy_=true;
                    auto initial=std::move(*restore_); restore_.reset();
                    lock.unlock();
                    scratch.reset(); scratch=workspace(); allowance=RuntimeAllowance{};
                    slot.frame.emplace(Frame{std::move(initial),std::nullopt,token,Clock::now()});
                    lock.lock();
                    if (!closing_) {
                        slot.frame->published=Clock::now(); slot.phase=Phase::published; head_=index; ready_=true;
                        status_.last=Code::ok; status_.exception_text={}; ++status_.published;
                    }
                    else slot.phase=Phase::retired;
                    status_.updates_left=allowance.remaining_updates(); status_.work_left=allowance.remaining_work();
                    busy_=false; cv_.notify_all(); continue;
                } else if (job_) {
                    const auto job=*job_; job_.reset(); busy_=true;
                    const auto* input=&slots_[job.input].frame->state;
                    lock.unlock();
                    if (hooks_.before_compute) hooks_.before_compute(hooks_.context, job.token);
                    const auto counts=CE::Memory::Detail::thread_allocations;
                    const auto begin=Clock::now();
                    Code code=Code::ok;
                    RegionalFailure failure{FailureKind::invalid_input,RegionalReason::weather,0,{}};
                    std::array<char,192> message{};
                    Index work=0;
                    try {
                        auto result=advance_runtime(*input,1,allowance,*scratch);
                        if (result) {
                            work=result->state.usage().work;
                            slots_[job.token.slot].frame.emplace(Frame{std::move(result->state),std::move(result->report),job.token,Clock::now()});
                        } else { code=Code::solver; failure=result.error(); work=failure.work; }
                    } catch (const std::exception& error) {
                        code=Code::exception; allowance=RuntimeAllowance{0,0};
                        static_cast<void>(std::snprintf(message.data(),message.size(),"%s",error.what()));
                    } catch (...) {
                        code=Code::exception; allowance=RuntimeAllowance{0,0};
                        static_cast<void>(std::snprintf(message.data(),message.size(),"non-standard weather exception"));
                    }
                    const auto elapsed=ms(begin,Clock::now());
                    if (hooks_.after_compute) hooks_.after_compute(hooks_.context,job.token);
                    lock.lock();
                    auto& slot=slots_[job.token.slot];
                    if (code==Code::ok && job.token.epoch==epoch_.load() && !closing_) {
                        slot.frame->published=Clock::now();
                        slot.phase=Phase::published; head_=job.token.slot; ++status_.published;
                    } else {
                        if (slot.frame) slot.phase=Phase::retired; else slot.phase=Phase::free;
                        if (job.token.epoch!=epoch_.load() || closing_) { ++status_.discarded; code=Code::stale; }
                    }
                    status_.last=code; status_.failure=failure; status_.exception_text=message;
                    status_.work_left=allowance.remaining_work(); status_.updates_left=allowance.remaining_updates();
                    timing({job.token,elapsed,0,work,difference(CE::Memory::Detail::thread_allocations,counts)});
                    busy_=false; cv_.notify_all(); continue;
                }
                cv_.wait(lock,[&] { return reclaimable() || (closing_ && pins_.load()==0) ||
                    (!closing_ && (job_.has_value() || (restore_.has_value() && free_slot()!=slot_count))); });
            }
        } catch (...) {
            // Fail closed, keeping permitted old pins readable until release.
            // Engine's noexcept final-return failure remains independently fatal.
            std::unique_lock lock(mutex_);
            closing_=true; busy_=false; status_.last=Code::exception;
            status_.producer=std::this_thread::get_id();
            try { throw; }
            catch (const std::exception& e) { static_cast<void>(std::snprintf(status_.exception_text.data(),status_.exception_text.size(),"%s",e.what())); }
            catch (...) { static_cast<void>(std::snprintf(status_.exception_text.data(),status_.exception_text.size(),"weather lifecycle exception")); }
            job_.reset();
            for (auto& slot:slots_) slot.phase=slot.frame ? Phase::retired : Phase::free;
            if (restore_) {
                auto discarded=std::move(*restore_); restore_.reset();
                lock.unlock(); { auto owned=std::move(discarded); static_cast<void>(owned); } lock.lock();
            }
            cv_.notify_all();
            while (pins_.load() || reclaimable()) {
                if (reclaimable()) {
                    for (std::size_t i=0;i<slot_count;++i) if (slots_[i].phase==Phase::retired && slots_[i].pins.load()==0) {
                        slots_[i].phase=Phase::reclaiming; lock.unlock(); reclaim(i); lock.lock(); break;
                    }
                } else cv_.wait(lock);
            }
            done_=true; cv_.notify_all();
        }
    }
}
