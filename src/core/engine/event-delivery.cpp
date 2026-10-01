#include <core/engine/event-delivery.h>

#include <utility>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>

namespace {
    struct WorkerStream {
        CE::Engine::WorkerGroup group;
        std::mutex mutex;
        std::deque<CE::SubSystems::EventBus::Work> pending;
        bool scheduled = false;

        explicit WorkerStream(CE::Engine::WorkerGroup value) : group(std::move(value)) {}
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

    struct WorkerPump {
        std::shared_ptr<WorkerStream> stream;
        std::atomic<bool> entered{false};
        bool published = false;

        explicit WorkerPump(std::shared_ptr<WorkerStream> value) : stream(std::move(value)) {}
        ~WorkerPump() {
            if (published && !entered.load(std::memory_order_acquire))
                abandon_stream(stream);
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
        }
        catch (...) {
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
        auto stream = std::make_shared<WorkerStream>(std::move(group));
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
                    pump->published = true;
                    try {
                        // Submit while holding the stream lock. Other producers
                        // may join only after this pump is actually accepted.
                        (void)stream->group.submit([pump] {
                            pump->entered.store(true, std::memory_order_release);
                            drain_stream(pump->stream);
                        });
                    } catch (...) {
                        // Only this request was published under the stream lock.
                        // Do not cancel another listener while its caller still
                        // holds an EventBus posting lock. Release our capture
                        // after unlocking; its bus ticket preserves the error.
                        rejected = std::move(stream->pending.front());
                        stream->pending.pop_front();
                        stream->scheduled = false;
                        pump->published = false;
                        throw;
                    }
                }
            }
            return true;
        };
    }
}
