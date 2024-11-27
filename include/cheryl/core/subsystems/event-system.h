#pragma once
#ifndef EVENT_SYSTEM_H
#define EVENT_SYSTEM_H
#include <any>
#include <vector>
#include <functional>
#include <shared_mutex>
#include <templates/singleton.h>

namespace CE::SubSystems {
    struct EventSystem : Singleton_CTS<EventSystem> {
        using Callback = std::function<void(std::any)>;
        EventSystem() = default;
        void dispatch(const std::string &event, const std::any &payload);
        void register_listener(const std::string &event, const Callback &callback);
    protected:
        std::shared_mutex mtx;
        std::unordered_map<std::string, std::vector<Callback>> event_listeners;
    };
}

#endif //EVENT_SYSTEM_H
