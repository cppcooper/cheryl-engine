#include "dungeon.h"
#include "dungeon-checks.h"

#include <iostream>
#include <stdexcept>

int main() {
    unsigned passed = 0;
    try {
        const std::array<ProcGen::Index, 2> extents{31, 21};
        const ProcGen::StreamKey key{42, 0x64756e67656f6e, 7};
        const ProcGen::CaveBudgets budget{4096, 1024 * 1024, 8'000'000};
        const ProcGen::DungeonConfiguration small{8, {2, 2}, {4, 4}, 1, 1, 256}, large{4, {5, 5}, {7, 7}, 2, 2, 256};
        for (const auto config : {small, large}) {
            const auto dungeon = ProcGen::generate_dungeon(extents, key, config, budget).value();
            const auto report = ProcGenDungeonChecks::inspect(dungeon);
            const auto m = dungeon.measurements();
            std::cout << "dungeon_rooms_and_chain v" << dungeon.descriptor().version << "; domain=31x21; index=x+width*y; seed=" << key.seed
                      << "; stage=" << key.stage << "; stream=" << key.stream << "; size=" << config.minimum[0] << ".." << config.maximum[0]
                      << "x" << config.minimum[1] << ".." << config.maximum[1] << "; spacing=" << config.spacing
                      << "; margin=" << config.margin << "; attempt_limit=" << config.attempts << "; requested=" << m.requested
                      << "; achieved=" << m.achieved
                      << "; achievement=" << (m.achievement == ProcGen::DungeonAchievement::complete ? "complete" : "attempt_limit")
                      << "; attempts=" << m.attempts << "; connections=" << dungeon.connections().size() << "; room_floor=" << m.room_floor
                      << "; corridor_floor=" << m.corridor_floor << "; total_floor=" << report.floor << "; components=" << report.components
                      << "; nonempty_connected=" << m.connected << "; work=" << dungeon.usage().work
                      << "; raw_draws=" << dungeon.usage().random_draws << "; independent_geometry_and_cardinal_checks=PASS\n";
            ++passed;
        }
        std::cout << "Dungeon consumer: " << passed << " passed, 0 failed, 0 skipped.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Dungeon consumer failure: " << error.what() << "\nDungeon consumer: " << passed << " passed, 1 failed, 0 skipped.\n";
        return 1;
    }
}
