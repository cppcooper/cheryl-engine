#pragma once

namespace CE {
    struct FramebufferSize {
        int width = 1;
        int height = 1;

        bool operator==(const FramebufferSize&) const = default;
    };
}
