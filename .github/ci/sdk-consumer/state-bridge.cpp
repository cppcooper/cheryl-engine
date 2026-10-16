#include "state-bridge.h"

#include <cheryl/core/logging/log.h>
#include <cheryl/core/subsystems/event-system.h>
#include <cheryl/templates/asset-mgr.h>

const void* sdk_event_system() {
    return &CE::Events::EventSystem::get();
}

const void* sdk_logging_mutex() {
    return &CE::Logging::Detail::default_logger_mutex;
}

bool sdk_provider_bound(const CE::Rendering::Resources::Provider& provider) {
    return CE::Assets::Cache::Context::is_bound_to(provider);
}

void sdk_throw_exception() {
    throw CE::Exceptions::runtime_exception(CE_HERE, "SDK bridge exception");
}
