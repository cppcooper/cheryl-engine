#pragma once

#include <tools/scenarios/runner.h>
#include <cheryl/tools/console/runtime/command.h>

namespace CE::LivingWorld::Scenarios {
    class Commands : public CE::Tools::Console::Runtime::Commands {
    public:
        Commands(Core::Registry& registry, Config config)
            : CE::Tools::Console::Runtime::Commands(registry, isolated_config(std::move(config))) {}
    };
}
