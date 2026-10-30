#pragma once

#include <core/controls/action-context.h>

#include <atomic>
#include <map>
#include <mutex>

namespace CE::Input::Actions::Detail {
    struct ContextToken {
        const ContextId id;
        std::atomic<bool> requested{true};
        explicit ContextToken(ContextId id)
        : id(id) {}
    };

    struct ContextsState {
        struct Entry {
            std::weak_ptr<ContextToken> token;
            std::shared_ptr<Bindings> bindings;
            bool removing = false;
        };
        std::mutex mutex;
        bool closed = false;
        std::map<ContextId, Entry> pending;
        // Only the polling owner accesses active entries.
        std::map<ContextId, Entry> active;
    };
}
