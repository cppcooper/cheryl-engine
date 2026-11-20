#pragma once

#include <host/host.h>

namespace CE::LivingWorld {
    struct HostTestAccess {
        static auto reserve(Host& host, const Host::Access& access, const Action& action) {
            return host.reserve(access, action, host.fence());
        }
        static const Detail::Image& image(const Host& host) noexcept { return *host.image_; }
    };
}
