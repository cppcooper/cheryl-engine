#pragma once

#include "input-record.h"

#include <memory>
#include <vector>

namespace CE::Input {
    /** One complete poll: State and independent ordered capture records.
     * Published as a const handle so later polls cannot mutate either channel.
     */
    struct PollSnapshot {
        std::shared_ptr<const Actions::Snapshot> state;
        std::vector<Events::Record> records;
    };
}
