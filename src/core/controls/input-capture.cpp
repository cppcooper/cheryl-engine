#include <core/controls/input-capture.h>

#include <internals/exceptions.h>

#include <utility>

namespace CE::Input {
    CaptureLease::CaptureLease(std::shared_ptr<Detail::CaptureCounts> counts, const InputMode mode)
        : counts_(std::move(counts)), mode_(mode) {
        if (mode == InputMode::Events)
            counts_->events.fetch_add(1);
        else if (mode == InputMode::Text)
            counts_->text.fetch_add(1);
    }

    CaptureLease::~CaptureLease() { reset(); }
    CaptureLease::CaptureLease(CaptureLease&& other) noexcept : counts_(std::move(other.counts_)), mode_(other.mode_) {}
    CaptureLease& CaptureLease::operator=(CaptureLease&& other) noexcept {
        if (this != &other) {
            reset();
            counts_ = std::move(other.counts_);
            mode_ = other.mode_;
        }
        return *this;
    }
    void CaptureLease::reset() noexcept {
        if (!counts_)
            return;
        if (mode_ == InputMode::Events)
            counts_->events.fetch_sub(1);
        else if (mode_ == InputMode::Text)
            counts_->text.fetch_sub(1);
        counts_.reset();
    }

    CaptureLease InputCapture::request(const InputMode mode) {
        switch (mode) {
            case InputMode::State:
            case InputMode::Events:
            case InputMode::Text:
                return CaptureLease(counts_, mode);
        }
        throw Exceptions::invalid_args(CE_HERE, "Unknown input capture channel");
    }

    void InputCapture::begin_poll(const KeyboardFocus focus) {
        events_enabled_ = counts_->events.load() != 0;
        text_enabled_ = counts_->text.load() != 0;
        focus_ = focus;
    }

    void
    InputCapture::record(const DeviceId device, const DeviceKind kind, InputRecordData data, const InputClock::time_point observed_at) {
        const auto* text = std::get_if<TextEvent>(&data);
        if (text ? !text_enabled_ : !events_enabled_)
            return;
        if (text && (text->codepoint > 0x10FFFF || (text->codepoint >= 0xD800 && text->codepoint <= 0xDFFF)))
            throw Exceptions::invalid_args(CE_HERE, "Text input requires a Unicode scalar value");
        const bool keyboard = kind == DeviceKind::Keyboard || text;
        pending_.push_back({next_sequence_++, observed_at, device, kind, std::move(data), keyboard ? focus_.target : 0,
                            keyboard ? focus_.epoch : 0,
                            !keyboard || focus_.target == 0 || focus_.routing == KeyboardRouting::PassThrough});
    }

    std::vector<InputRecord> InputCapture::complete() { return std::exchange(pending_, {}); }
    void InputCapture::discard_pending() {
        pending_.clear();
        events_enabled_ = false;
        text_enabled_ = false;
        focus_ = {};
    }
}
