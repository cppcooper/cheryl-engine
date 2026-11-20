#include "state.h"
#include <algorithm>

namespace CE::Story {
    std::expected<ActorView, ActorError> ActorEndpoint::view(std::size_t cursor, std::size_t limit) const {
        const auto state = state_.lock();
        if (!state) { return std::unexpected(ActorError::Expired); }
        if (!state->available) { return std::unexpected(ActorError::Unavailable); }
        if (limit == 0 || limit > 64 || cursor > state->view.account_count) {
            return std::unexpected(ActorError::Invalid);
        }
        auto result = state->view;
        result.total_accounts = result.account_count;
        result.account_count = std::min(limit, result.total_accounts - cursor);
        result.accounts = {};
        for (std::size_t index = 0; index < result.account_count; ++index) {
            result.accounts[index] = state->view.accounts[cursor + index];
        }
        result.cursor = cursor + result.account_count;
        return result;
    }
    std::expected<ActorReceipt, ActorError> ActorEndpoint::submit(ActorAttemptKey key, const ActorChoice& choice) const {
        const auto state = state_.lock();
        if (!state) { return std::unexpected(ActorError::Expired); }
        if (!key.value || key.value <= state->retired) { return std::unexpected(ActorError::Expired); }
        for (std::size_t index = 0; index < state->entry_count; ++index) {
            const auto& entry = state->entries[index];
            if (entry.key == key) {
                if (entry.choice != choice) { return std::unexpected(ActorError::Conflict); }
                return entry.receipt;
            }
        }
        bool found = false;
        for (std::size_t index = 0; index < state->view.offer_count; ++index) {
            const auto& offer = state->view.offers[index];
            if (offer.reference == choice.offer && offer.intent == choice.intent) { found = true; break; }
        }
        if (!found || !Detail::valid(*state, choice.intent)) { return std::unexpected(ActorError::Invalid); }
        for (std::size_t index = 0; index < state->entry_count; ++index) {
            if (state->entries[index].receipt.outcome == ActorOutcome::Pending) {
                return std::unexpected(ActorError::Busy);
            }
        }
        if (!state->available) { return std::unexpected(ActorError::Unavailable); }
        if (state->entry_count == state->entries.size()) { return std::unexpected(ActorError::Limit); }
        auto& entry = state->entries[state->entry_count++];
        entry.key = key;
        entry.choice = choice;
        entry.receipt = {key, ActorOutcome::Pending, {}};
        return entry.receipt;
    }
    std::expected<ActorReceipt, ActorError> ActorEndpoint::lookup(ActorAttemptKey key) const {
        const auto state = state_.lock();
        if (!state) { return std::unexpected(ActorError::Expired); }
        if (!key.value || key.value <= state->retired) { return std::unexpected(ActorError::Expired); }
        for (std::size_t index = 0; index < state->entry_count; ++index) {
            if (state->entries[index].key == key) { return state->entries[index].receipt; }
        }
        return std::unexpected(ActorError::Unknown);
    }
    std::expected<void, ActorError> ActorEndpoint::retire_through(ActorAttemptKey key) const {
        const auto state = state_.lock();
        if (!state) { return std::unexpected(ActorError::Expired); }
        if (key.value < state->retired) { return std::unexpected(ActorError::Expired); }
        for (std::size_t index = 0; index < state->entry_count; ++index) {
            const auto& entry = state->entries[index];
            if (entry.key.value <= key.value && entry.receipt.outcome == ActorOutcome::Pending) {
                return std::unexpected(ActorError::Busy);
            }
        }
        state->retired = key.value;
        std::size_t retained = 0;
        for (std::size_t index = 0; index < state->entry_count; ++index) {
            if (state->entries[index].key.value > key.value) {
                state->entries[retained++] = state->entries[index];
            }
        }
        for (auto index = retained; index < state->entry_count; ++index) { state->entries[index] = {}; }
        state->entry_count = retained;
        return {};
    }
}
