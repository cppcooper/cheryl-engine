#pragma once

#include <host/image.h>

namespace CE::LivingWorld {
    enum class Source { Eyewitness };
    struct Evidence {
        std::uint64_t id = 0;
        EntityRef subject;
        Source source = Source::Eyewitness;
        std::uint64_t observed_at = 0, acquired_at = 0;
        bool operator==(const Evidence&) const = default;
    };
    struct Cursor {
        EntityRef recipient;
        OwnerFence incarnation;
        std::uint64_t after = 0;
    };
    struct Page {
        std::array<Evidence, 32> wound_closures{};
        std::size_t size = 0;
        Cursor next;
    };
    struct Delivery {
        std::size_t adopted = 0;
        bool backpressure = false, withheld = false;
    };
}
