#include "state-bridge.h"

#include <cheryl/core/logging/logger.h>
#include <cheryl/core/subsystems/event-system.h>
#include <cheryl/templates/asset-mgr.h>

const void* sdk_event_system() {
    return &CE::Events::EventSystem::get();
}

namespace {
    constexpr char sdk_category[] = "sdk-custom";
}

std::uint16_t sdk_logging_id() {
    return CE::Logging::Logger<sdk_category>::get().get_log_id();
}

const void* sdk_logging_handle() {
    return CE::Logging::Logger<sdk_category>::get_existing();
}

void sdk_logging_quiet() {
    CE::Logging::Logger<sdk_category>::set_level_logger(CE::Logging::Level::off);
}

bool sdk_logging_accepts_warning() {
    return CE::Logging::Logger<sdk_category>::should_log(CE::Logging::Level::warn);
}

bool sdk_provider_bound(const CE::Rendering::Resources::Provider& provider) {
    return CE::Assets::Cache::Context::is_bound_to(provider);
}

void sdk_throw_exception() {
    throw CE::Exceptions::runtime_exception(CE_HERE, "SDK bridge exception");
}
