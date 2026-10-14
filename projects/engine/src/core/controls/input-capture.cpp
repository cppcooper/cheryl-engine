#include <core/controls/input-capture.h>

#include <internals/exceptions.h>

#include <utility>

namespace CE::Input::Capture {
    Lease::Lease(std::shared_ptr<Detail::Counts> counts, const Channel mode)
    : counts_(std::move(counts)), mode_(mode) {
        if (mode == Channel::Events)
            counts_->events.fetch_add(1);
        else if (mode == Channel::Text)
            counts_->text.fetch_add(1);
    }

    Lease::~Lease() {
        reset();
    }

    Lease::Lease(Lease&& other) noexcept
    : counts_(std::move(other.counts_)), mode_(other.mode_) {}

    Lease& Lease::operator=(Lease&& other) noexcept {
        if (this != &other) {
            reset();
            counts_ = std::move(other.counts_);
            mode_ = other.mode_;
        }
        return *this;
    }

    void Lease::reset() noexcept {
        if (!counts_)
            return;
        if (mode_ == Channel::Events)
            counts_->events.fetch_sub(1);
        else if (mode_ == Channel::Text)
            counts_->text.fetch_sub(1);
        counts_.reset();
    }

    Lease Collector::request(const Channel mode) {
        switch (mode) {
            case Channel::State:
            case Channel::Events:
            case Channel::Text:
                return Lease(counts_, mode);
        }
        throw Exceptions::invalid_args(CE_HERE, "Unknown input capture channel");
    }

    void Collector::begin_poll(const CE::Input::Routing::Focus focus) {
        events_enabled_ = counts_->events.load() != 0;
        text_enabled_ = counts_->text.load() != 0;
        focus_ = focus;
    }

    void Collector::record(
        const CE::Input::DeviceId device,
        const CE::Input::DeviceKind kind,
        CE::Input::Events::Payload data,
        const CE::Input::ObservationClock::time_point observed_at
    ) {
        const auto* text = std::get_if<CE::Input::Events::Text>(&data);
        if (text ? !text_enabled_ : !events_enabled_)
            return;
        if (text && (text->codepoint > 0x10FFFF || (text->codepoint >= 0xD800 && text->codepoint <= 0xDFFF)))
            throw Exceptions::invalid_args(CE_HERE, "Text input requires a Unicode scalar value");
        const bool keyboard = kind == CE::Input::DeviceKind::Keyboard || text;
        pending_.push_back(
            {next_sequence_++, observed_at, device, kind, std::move(data), keyboard ? focus_.target : 0, keyboard ? focus_.epoch : 0,
             !keyboard || focus_.target == 0 || focus_.routing == CE::Input::Routing::KeyboardPolicy::PassThrough}
        );
    }

    std::vector<CE::Input::Events::Record> Collector::complete() {
        return std::exchange(pending_, {});
    }

    void Collector::discard_pending() {
        pending_.clear();
        events_enabled_ = false;
        text_enabled_ = false;
        focus_ = {};
    }
}
