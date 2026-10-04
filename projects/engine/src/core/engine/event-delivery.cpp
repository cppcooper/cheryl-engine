#include <core/engine/event-delivery.h>
#include "event-delivery-internal.h"

#include <utility>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>

namespace {
    struct WorkerStream {
        CE::Engine::WorkerGroup group;
        CE::Engine::DeliveryDetail::WorkerSubmission submit;
        std::mutex mutex;
        std::deque<CE::SubSystems::EventBus::Work> pending;
        bool scheduled = false;

        WorkerStream(CE::Engine::WorkerGroup value, CE::Engine::DeliveryDetail::WorkerSubmission submission)
        : group(std::move(value)), submit(std::move(submission)) {}
    };

    void abandon_stream(const std::shared_ptr<WorkerStream>& stream) {
        std::deque<CE::SubSystems::EventBus::Work> cancelled;
        {
            std::lock_guard lock(stream->mutex);
            stream->scheduled = false;
            cancelled.swap(stream->pending);
        }
        // Event delivery tickets report cancellation when these captures die.
        // Release them outside the stream lock so error sinks may reenter.
    }

    enum class PumpPhase { Preparing, Accepted, Cancelled };

    struct WorkerPump {
        std::shared_ptr<WorkerStream> stream;
        std::atomic<PumpPhase> phase{PumpPhase::Preparing};

        explicit WorkerPump(std::shared_ptr<WorkerStream> value)
        : stream(std::move(value)) {}
    };

    // Only the submitted callable owns cancellation cleanup. A producer's local
    // shared pump owner must never cancel another listener under its posting lock.
    struct WorkerPumpJob {
        std::shared_ptr<WorkerPump> pump;
        bool entered = false;

        explicit WorkerPumpJob(std::shared_ptr<WorkerPump> value)
        : pump(std::move(value)) {}
        WorkerPumpJob(WorkerPumpJob&&) noexcept = default;
        WorkerPumpJob(const WorkerPumpJob&) = delete;
        ~WorkerPumpJob() {
            if (!pump || entered)
                return;
            // Before acceptance, record loss without taking the stream mutex:
            // synchronous submit failure may already hold it. The producer
            // withdraws its sole request before another producer can join.
            if (pump->phase.exchange(PumpPhase::Cancelled, std::memory_order_acq_rel) == PumpPhase::Accepted)
                abandon_stream(pump->stream);
        }
    };

    void drain_stream(const std::shared_ptr<WorkerStream>& stream) {
        try {
            while (true) {
                CE::SubSystems::EventBus::Work work;
                {
                    std::lock_guard lock(stream->mutex);
                    if (stream->pending.empty()) {
                        stream->scheduled = false;
                        return;
                    }
                    work = std::move(stream->pending.front());
                    stream->pending.pop_front();
                }
                // Only one pump owns a stream. Parallel pool jobs therefore
                // cannot reorder the completion of these callbacks.
                work();
            }
        } catch (...) {
            abandon_stream(stream);
            throw;
        }
    }
}

namespace CE::Engine {
    SubSystems::EventBus::Delivery platform_event_delivery(PlatformDispatcher::Submission endpoint) {
        return [endpoint = std::move(endpoint)](SubSystems::EventBus::Work work) {
            (void)endpoint.submit([work = std::move(work)](EngineContext&) mutable { work(); });
            return true;
        };
    }

    SubSystems::EventBus::Delivery simulation_event_delivery(SimulationDispatcher::Submission endpoint) {
        return [endpoint = std::move(endpoint)](SubSystems::EventBus::Work work) {
            (void)endpoint.submit(std::move(work));
            return true;
        };
    }

    SubSystems::EventBus::Delivery worker_event_delivery(WorkerGroup group) {
        auto submit = [group](SubSystems::EventBus::Work work) { (void)group.submit(std::move(work)); };
        return DeliveryDetail::worker_stream_delivery(std::move(group), std::move(submit));
    }

    SubSystems::EventBus::Delivery DeliveryDetail::worker_stream_delivery(WorkerGroup group, DeliveryDetail::WorkerSubmission submit) {
        auto stream = std::make_shared<WorkerStream>(std::move(group), std::move(submit));
        return [stream](SubSystems::EventBus::Work work) {
            std::shared_ptr<WorkerPump> pump;
            SubSystems::EventBus::Work rejected;
            {
                std::lock_guard lock(stream->mutex);
                // Check closure while excluding the pump's final empty-queue
                // check. Accepted work cannot appear after that pump completes.
                if (!stream->group.status().accepting)
                    return false;
                if (!stream->scheduled)
                    pump = std::make_shared<WorkerPump>(stream);
                stream->pending.push_back(std::move(work));
                if (pump) {
                    stream->scheduled = true;
                    try {
                        // Submit while holding the stream lock. Other producers
                        // may join only after this pump is actually accepted.
                        stream->submit([job = WorkerPumpJob(pump)]() mutable {
                            job.entered = true;
                            drain_stream(job.pump->stream);
                        });
                        auto expected = PumpPhase::Preparing;
                        if (!pump->phase.compare_exchange_strong(expected, PumpPhase::Accepted, std::memory_order_acq_rel)) {
                            // Policy rejection already destroyed the unentered
                            // job. No other request can have joined this pump.
                            rejected = std::move(stream->pending.front());
                            stream->pending.pop_front();
                            stream->scheduled = false;
                            return false;
                        }
                    } catch (...) {
                        // Only this request was published under the stream lock.
                        // Do not cancel another listener while its caller still
                        // holds an EventBus posting lock. Release our capture
                        // after unlocking; its bus ticket preserves the error.
                        rejected = std::move(stream->pending.front());
                        stream->pending.pop_front();
                        stream->scheduled = false;
                        throw;
                    }
                }
            }
            return true;
        };
    }
}
