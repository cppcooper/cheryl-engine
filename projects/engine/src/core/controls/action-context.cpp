#include <core/controls/action-context.h>

#include "action-context-internal.h"
#include <core/diagnostics.h>
#include <internals/exceptions.h>

#include <utility>

namespace CE::Input::Actions {
    Context::Context(std::shared_ptr<Detail::ContextToken> token)
    : token_(std::move(token)) {}
    Context::~Context() {
        reset();
    }
    Context::Context(Context&& other) noexcept
    : token_(std::move(other.token_)) {}
    Context& Context::operator=(Context&& other) noexcept {
        if (this != &other) {
            reset();
            token_ = std::move(other.token_);
        }
        return *this;
    }
    void Context::reset() noexcept {
        if (token_)
            token_->requested.store(false, std::memory_order_release);
        token_.reset();
    }
    Context::operator bool() const noexcept {
        return token_ && token_->requested.load(std::memory_order_acquire);
    }
    ContextId Context::id() const noexcept {
        return token_ ? token_->id : 0;
    }

    ContextSubmission::ContextSubmission(std::shared_ptr<Detail::ContextsState> state)
    : state_(std::move(state)) {}
    Context ContextSubmission::attach(ContextDefinition definition) const {
        if (!state_)
            throw Exceptions::failed_operation(CE_HERE, "Input action-context endpoint is empty");
        auto mapper = std::make_shared<Bindings>();
        for (auto& button : definition.buttons)
            static_cast<void>(mapper->bind_button(std::move(button.chord), button.action));
        for (auto& axis : definition.axes)
            static_cast<void>(mapper->bind_axis(std::move(axis.modifiers), axis.control, axis.action, axis.options));
        auto token = std::make_shared<Detail::ContextToken>(CE::Diagnostics::next_domain_id());
        const std::lock_guard lock(state_->mutex);
        if (state_->closed)
            throw Exceptions::failed_operation(CE_HERE, "Input action-context endpoint is closed");
        state_->pending.emplace(token->id, Detail::ContextsState::Entry{token, std::move(mapper)});
        return Context(std::move(token));
    }
}
