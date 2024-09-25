#pragma once
#include "celog.h"
#include <math/pointers.h>

namespace CE::ptr {
    template <typename Target, typename Source>
    std::shared_ptr<Target> make_alias(const std::shared_ptr<Source>& source) {
        // Create an aliasing shared_ptr
        return std::shared_ptr<Target>(source, reinterpret_cast<Target*>(source.get()));
    }
    template <typename Target, typename Source>
    std::shared_ptr<Target> make_alias(const std::shared_ptr<Source>& source, const std::size_t address_offset) {
        // Create an aliasing shared_ptr
        return std::shared_ptr<Target>(source, ptr::add_offset<Target>(source.get(), address_offset));
    }
    template<class T, class U>
    std::shared_ptr<T> make_child(std::shared_ptr<U> parent, const std::function<void(const T*)>& deletor) {
        return std::shared_ptr<T>(
            reinterpret_cast<T*>(parent.get()),
            [p = parent, d = deletor](const T* ptr) {
                if (p.get()) {
                    d(ptr);
                } else {
                    CELog::critical("We've made it to unreachable code.");
                    std::unreachable();
                }
            }
        );
    }
    template<class T, class U>
    std::shared_ptr<T> make_child(void* head, std::shared_ptr<U> parent, const std::function<void(const T*)>& deletor) {
        return std::shared_ptr<T>(
            static_cast<T*>(head),
            [p = parent, d = deletor](const T* ptr) {
                if (p.get()) {
                    d(ptr);
                } else {
                    CELog::critical("We've made it to unreachable code.");
                    std::unreachable();
                }
            }
        );
    }
}
