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
            // Acceptance cannot rely solely on an already-running pump: group
            // closure must reject new stream work even while that pump drains.
            if (!stream->group.status().accepting)
                return false;
            std::shared_ptr<WorkerPump> pump;
            {
                std::lock_guard lock(stream->mutex);
                if (!stream->scheduled) {
                    // Allocate before publishing anything which needs a pump.
                    pump = std::make_shared<WorkerPump>(stream);
                }
                stream->pending.push_back(std::move(work));
                stream->scheduled = true;
                if (pump)
                    pump->published = true;
            }
            if (pump) {
                (void)stream->group.submit([pump] {
                    pump->entered.store(true, std::memory_order_release);
                    drain_stream(pump->stream);
                });
            }
            return true;
        };
    }
}
