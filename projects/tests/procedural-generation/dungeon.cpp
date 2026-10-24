#include "dungeon.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ProcGen {
    namespace {
        struct Stop {
            Failure failure;
        };
        [[noreturn]] void fail(FailureKind kind, Reason reason) {
            throw Stop{{kind, reason}};
        }
        Index add(Index a, Index b) {
            if (a > std::numeric_limits<Index>::max() - b) {
                fail(FailureKind::invalid_input, Reason::overflow);
            }
            return a + b;
        }
        Index multiply(Index a, Index b) {
            if (b != 0 && a > std::numeric_limits<Index>::max() / b) {
                fail(FailureKind::invalid_input, Reason::overflow);
            }
            return a * b;
        }
        void charge(Work& work, Index count = 1) {
            if (!work.consume(count)) {
                fail(FailureKind::budget_refused, Reason::work);
            }
        }
        std::array<Index, 2> available(const Domain& domain, Index margin) {
            std::array<Index, 2> result{};
            for (std::size_t axis = 0; axis < 2; ++axis) {
                const auto extent = domain.extents()[axis];
                result[axis] = margin > extent / 2 ? 0 : extent - 2 * margin;
            }
            return result;
        }
        DungeonRequirements needed_for(const Domain& domain, DungeonConfiguration config) {
            if (domain.dimensions() != 2) {
                fail(FailureKind::invalid_input, Reason::dungeon_dimensions);
            }
            if (config.rooms > dungeon_max_rooms || config.attempts > dungeon_max_attempts || config.margin == 0) {
                fail(FailureKind::invalid_input, Reason::dungeon_options);
            }
            for (std::size_t axis = 0; axis < 2; ++axis) {
                if (config.minimum[axis] == 0 || config.maximum[axis] < config.minimum[axis]) {
                    fail(FailureKind::invalid_input, Reason::dungeon_options);
                }
            }
            const auto space = available(domain, config.margin);
            if (config.rooms != 0 && (config.minimum[0] > space[0] || config.minimum[1] > space[1])) {
                fail(FailureKind::contradiction, Reason::dungeon_fit);
            }
            const auto cells = domain.cells();
            if (cells > std::vector<Index>{}.max_size() || cells > std::vector<std::uint8_t>{}.max_size()) {
                fail(FailureKind::invalid_input, Reason::overflow);
            }
            return {cells, add(add(fixed_storage_allowance, multiply(10, cells)), multiply(80, config.rooms)), multiply(4, cells)};
        }
        bool separated(const DungeonRoom& a, const DungeonRoom& b, Index spacing) {
            for (std::size_t axis = 0; axis < 2; ++axis) {
                if ((a.upper[axis] <= b.lower[axis] && b.lower[axis] - a.upper[axis] >= spacing) ||
                    (b.upper[axis] <= a.lower[axis] && a.lower[axis] - b.upper[axis] >= spacing)) {
                    return true;
                }
            }
            return false;
        }
        std::array<Index, 2> center(const DungeonRoom& room) {
            return {room.lower[0] + (room.upper[0] - room.lower[0] - 1) / 2, room.lower[1] + (room.upper[1] - room.lower[1] - 1) / 2};
        }
        static_assert(sizeof(DungeonRoom) == 40 && sizeof(DungeonConnection) == 40);
        static_assert(2 * sizeof(Dungeon) + 2 * sizeof(DungeonReplay) + 1024 <= fixed_storage_allowance);
    }
    Result<DungeonRequirements> dungeon_requirements(const Domain& domain, DungeonConfiguration config) {
        try {
            return needed_for(domain, config);
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }
    Dungeon::Dungeon(
        DungeonReplay replay,
        std::vector<DungeonRoom> rooms,
        std::vector<DungeonConnection> connections,
        std::vector<std::uint8_t> occupancy,
        DungeonMeasurements measured,
        Usage usage
    )
    : replay_(std::move(replay)),
      rooms_(std::move(rooms)),
      connections_(std::move(connections)),
      occupancy_(std::move(occupancy)),
      measured_(measured),
      usage_(usage) {}
    Result<Dungeon> replay_dungeon(const DungeonReplay& replay) {
        try {
            if (replay.algorithm != DungeonAlgorithm::rooms_and_chain) {
                fail(FailureKind::unsupported, Reason::dungeon_algorithm);
            }
            if (replay.version != 1) {
                fail(FailureKind::unsupported, Reason::dungeon_version);
            }
            const auto& domain = replay.domain;
            const auto config = replay.requested;
            const auto needed = needed_for(domain, config);
            const CaveBudgets budget{
                std::min(replay.budgets.cells, dungeon_hard_limits.cells),
                std::min(replay.budgets.storage_bytes, dungeon_hard_limits.storage_bytes),
                std::min(replay.budgets.work, dungeon_hard_limits.work)
            };
            if (needed.cells > budget.cells) {
                fail(FailureKind::budget_refused, Reason::cells);
            }
            if (needed.storage_bytes > budget.storage_bytes) {
                fail(FailureKind::budget_refused, Reason::storage);
            }
            if (needed.minimum_work > budget.work) {
                fail(FailureKind::budget_refused, Reason::work);
            }
            Work work(budget.work);
            Usage usage{};
            charge(work, multiply(3, needed.cells));
            const auto size = static_cast<std::size_t>(needed.cells);
            std::vector<std::uint8_t> occupancy(size, 1), visited(size, 0);
            std::vector<Index> queue(size, 0);
            std::vector<DungeonRoom> rooms;
            rooms.reserve(config.rooms);
            std::vector<DungeonConnection> connections;
            connections.reserve(config.rooms);
            const auto base = derive_stream_state(replay.random);
            auto placement = IntegerStream::for_stage({base, dungeon_placement_stage, 0});
            auto corridors = IntegerStream::for_stage({base, dungeon_corridor_stage, 0});
            const auto draw = [&](IntegerStream& stream, Index bound) {
                const auto before = work.used();
                const auto sampled = sample_below(stream, bound, work);
                usage.random_draws = add(usage.random_draws, work.used() - before);
                if (!sampled) {
                    throw Stop{sampled.error()};
                }
                return *sampled;
            };
            const auto space = available(domain, config.margin);
            DungeonMeasurements measured;
            measured.requested = config.rooms;
            while (rooms.size() < config.rooms && measured.attempts < config.attempts) {
                charge(work);
                ++measured.attempts;
                std::array<Index, 2> extent{};
                for (std::size_t axis = 0; axis < 2; ++axis) {
                    extent[axis] = add(config.minimum[axis], draw(placement, add(config.maximum[axis] - config.minimum[axis], 1)));
                }
                if (extent[0] > space[0] || extent[1] > space[1]) {
                    ++measured.fit_rejections;
                    continue;
                }
                DungeonRoom candidate{static_cast<Index>(rooms.size()), {}, {}};
                for (std::size_t axis = 0; axis < 2; ++axis) {
                    candidate.lower[axis] = add(config.margin, draw(placement, add(space[axis] - extent[axis], 1)));
                    candidate.upper[axis] = add(candidate.lower[axis], extent[axis]);
                }
                bool clear = true;
                for (const auto& room : rooms) {
                    charge(work);
                    if (!separated(room, candidate, config.spacing)) {
                        clear = false;
                        break;
                    }
                }
                if (!clear) {
                    ++measured.overlap_rejections;
                    continue;
                }
                charge(work);
                rooms.push_back(candidate);
            }
            measured.achieved = static_cast<Index>(rooms.size());
            measured.achievement = rooms.size() == config.rooms ? DungeonAchievement::complete : DungeonAchievement::attempt_limit;
            const auto width = domain.extents()[0], height = domain.extents()[1];
            const auto carve = [&](std::array<Index, 2> cell) {
                charge(work);
                auto& value = occupancy[static_cast<std::size_t>(cell[1] * width + cell[0])];
                const auto was_solid = value;
                value = 0;
                return static_cast<Index>(was_solid);
            };
            for (const auto& room : rooms) {
                charge(work);
                for (Index y = room.lower[1]; y < room.upper[1]; ++y) {
                    for (Index x = room.lower[0]; x < room.upper[0]; ++x) {
                        measured.room_floor += carve({x, y});
                    }
                }
            }
            for (std::size_t i = 1; i < rooms.size(); ++i) {
                charge(work);
                auto cell = center(rooms[i - 1]);
                const auto goal = center(rooms[i]);
                DungeonConnection connection{static_cast<Index>(i - 1), static_cast<Index>(i), draw(corridors, 2), 0, 0};
                connection.carved += carve(cell);
                for (std::size_t leg = 0; leg < 2; ++leg) {
                    const auto axis = static_cast<std::size_t>((connection.first_axis + leg) % 2);
                    while (cell[axis] != goal[axis]) {
                        if (cell[axis] < goal[axis]) {
                            ++cell[axis];
                        } else {
                            --cell[axis];
                        }
                        connection.carved += carve(cell);
                        ++connection.length;
                    }
                }
                measured.corridor_floor = add(measured.corridor_floor, connection.carved);
                connections.push_back(connection);
            }
            Index first = needed.cells;
            for (Index cell = 0; cell < needed.cells; ++cell) {
                charge(work);
                const auto x = cell % width, y = cell / width;
                const auto value = occupancy[static_cast<std::size_t>(cell)];
                if ((x == 0 || y == 0 || x == width - 1 || y == height - 1) && value != 1) {
                    throw std::logic_error("Dungeon boundary invariant");
                }
                if (value == 0) {
                    ++measured.floor;
                    first = std::min(first, cell);
                }
            }
            Index head = 0, tail = 0;
            const auto visit = [&](Index cell) {
                charge(work);
                if (tail >= needed.cells) {
                    throw std::logic_error("Dungeon validation queue invariant");
                }
                visited[static_cast<std::size_t>(cell)] = 1;
                queue[static_cast<std::size_t>(tail++)] = cell;
            };
            if (first < needed.cells) {
                visit(first);
            }
            while (head < tail) {
                charge(work);
                const auto cell = queue[static_cast<std::size_t>(head++)];
                for (std::size_t direction = 0; direction < 4; ++direction) {
                    charge(work);
                    const auto next = domain.neighbor(cell, direction / 2, static_cast<Direction>(direction % 2)).value();
                    if (next && occupancy[static_cast<std::size_t>(*next)] == 0 && visited[static_cast<std::size_t>(*next)] == 0) {
                        visit(*next);
                    }
                }
            }
            if (tail != measured.floor || measured.room_floor + measured.corridor_floor != measured.floor) {
                throw std::logic_error("Dungeon occupancy connectivity/count invariant");
            }
            measured.solid = needed.cells - measured.floor;
            measured.connected = measured.floor != 0;
            usage.work = work.used();
            return Dungeon(replay, std::move(rooms), std::move(connections), std::move(occupancy), measured, usage);
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }
    Result<Dungeon> generate_dungeon(std::span<const Index> extents, StreamKey random, DungeonConfiguration config, CaveBudgets budget) {
        const auto domain = Domain::checked(extents);
        if (!domain) {
            return std::unexpected(domain.error());
        }
        return replay_dungeon({*domain, random, config, budget});
    }
}
