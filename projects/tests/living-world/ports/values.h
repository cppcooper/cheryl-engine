#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace CE::LivingWorld {
    struct WorldScope {
        std::uint64_t world = 1, branch = 1;
        bool operator==(const WorldScope&) const = default;
    };
    struct EntityRef {
        WorldScope scope;
        std::uint64_t id = 0, generation = 1;
        bool operator==(const EntityRef&) const = default;
    };
    struct OwnerFence {
        std::uint64_t instance = 1, epoch = 1;
        bool operator==(const OwnerFence&) const = default;
    };
    struct Stamp {
        WorldScope scope;
        std::uint64_t owner = 0, revision = 0;
        bool operator==(const Stamp&) const = default;
    };
    struct ReadSet {
        std::array<Stamp, 8> stamps{};
        std::size_t size = 0;
        bool operator==(const ReadSet&) const = default;
    };
    struct AttemptKey {
        WorldScope scope;
        std::uint64_t issuer = 0, sequence = 0;
        bool operator==(const AttemptKey&) const = default;
    };
}
