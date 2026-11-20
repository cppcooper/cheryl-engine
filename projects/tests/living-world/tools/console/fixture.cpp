#include <tools/console/fixture.h>
#include <consumer/fixture.h>

namespace CE::LivingWorld::Console {
    Practice practice(WorldScope scope) {
        auto catalog = fixture_catalog();
        auto plan = fixture_plan(catalog);
        auto host = std::make_shared<Host>(catalog, plan, scope, 20, 3);
        auto access = host->grant(host->entity(1));
        auto console = std::make_shared<Adapter>(host, plan, access);
        return {std::move(catalog), std::move(plan), std::move(host), std::move(access), std::move(console)};
    }
}
