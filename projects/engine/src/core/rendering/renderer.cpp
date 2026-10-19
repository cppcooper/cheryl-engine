#include <core/rendering/renderer.h>

#include <internals/failure-reporting.h>

#include <utility>

namespace CE::Rendering {
    namespace {
        struct TerminalObservation {
            CE::Events::EventBus::Publisher publisher;
            GraphicsDomainStateChanged change;
            bool entered = false;

            ~TerminalObservation() {
                if (!entered)
                    CE::Diagnostics::report_outcome("graphics_domain", change.current.id, "terminal_notification", "cancelled");
            }
        };
    }

    void Renderer::publish_events_to(CE::Events::EventBus::Publisher publisher, CE::Events::EventBus::Delivery terminal_delivery) noexcept {
        event_publisher_ = std::move(publisher);
        terminal_delivery_ = std::move(terminal_delivery);
    }

    void Renderer::publish_graphics_domain_changes(const bool destroying) noexcept {
        if (publishing_domain_)
            return;
        publishing_domain_ = true;
        // There can be only two transitions. Reentrant shutdown waits until the
        // Available fanout finishes, and cannot generate an unbounded drain loop.
        for (unsigned int remaining = 2; remaining != 0; --remaining) {
            const auto domain = graphics_domain();
            auto snapshot = domain.snapshot();
            if (snapshot.sequence <= published_sequence_)
                break;
            if (!destroying && published_sequence_ == 0 && snapshot.sequence == 2)
                snapshot = {snapshot.id, 1, GraphicsDomainStatus::Available, GraphicsDomainReason::Initialized};
            published_sequence_ = snapshot.sequence;
            if (!destroying) {
                // A listener may replace routing. Keep the selected handle alive
                // independently of reentrant member assignment.
                const auto publisher = event_publisher_;
                publisher.notify(graphics_domain_state_changed_event, GraphicsDomainStateChanged{snapshot, domain});
            } else if (!event_publisher_.expired() && terminal_delivery_) {
                try {
                    auto record = std::make_unique<TerminalObservation>();
                    record->publisher = event_publisher_;
                    record->change = {snapshot, domain};
                    if (!terminal_delivery_([record = std::move(record)] {
                            record->entered = true;
                            record->publisher.notify(graphics_domain_state_changed_event, record->change);
                        }))
                        CE::Diagnostics::report_outcome("graphics_domain", snapshot.id, "terminal_notification", "rejected");
                } catch (...) {
                    CE::Diagnostics::report_failure("graphics domain terminal notification", std::current_exception());
                }
            }
        }
        publishing_domain_ = false;
    }
}
