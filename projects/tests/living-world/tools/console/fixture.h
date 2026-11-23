#pragma once

#include <tools/console/adapter.h>

namespace CE::LivingWorld::Console {
    struct Practice {
        ::Magic::Catalog catalog;
        ::Magic::PlanHandle plan;
        std::shared_ptr<Host> host;
        Host::Access access;
        std::shared_ptr<Adapter> console;
    };
    Practice practice(WorldScope scope = {});
    ::Magic::Catalog practice_catalog();
    Practice practice(::Magic::Catalog, ::Magic::PlanHandle, WorldScope scope = {});
}
