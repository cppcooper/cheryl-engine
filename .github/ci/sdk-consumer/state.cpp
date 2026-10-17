#include "state-bridge.h"

#include <cheryl/assets/resources/resource-provider.h>
#include <cheryl/core/logging/logger.h>
#include <cheryl/core/logging/process-lifetime.h>
#include <cheryl/core/subsystems/event-system.h>
#include <cheryl/templates/asset-mgr.h>

#include <memory>
#include <string_view>

namespace {
    constexpr char sdk_category[] = "sdk-custom";
    struct MemoryProvider final : CE::Rendering::Resources::Provider {
        std::shared_ptr<CE::Rendering::Resources::Image> create_image(const CE::Assets::Images::Decoded&) override { return {}; }
        std::shared_ptr<CE::Rendering::Resources::Image>
        create_font_atlas(std::span<const unsigned char>, CE::Rendering::Geometry::PixelSize) override {
            return {};
        }
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D>
        upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2D>, CE::Rendering::Geometry::Topology) override {
            return {};
        }
        std::shared_ptr<CE::Rendering::Resources::Program> link_program(const std::vector<std::filesystem::path>&) override { return {}; }
    };

    struct CacheBinding : CE::Assets::Cache::Context {
        using Context::bind_provider;
    };
}

int main() {
    CE::Logging::ProcessLifetime logging_lifetime;
    const bool singleton = sdk_event_system() == &CE::Events::EventSystem::get();
    const bool logging_identity = sdk_logging_id() == CE::Logging::Logger<sdk_category>::get().get_log_id() &&
                                  sdk_logging_handle() == CE::Logging::Logger<sdk_category>::get_existing();
    sdk_logging_quiet();
    const bool logging_quiet = !CE::Logging::Logger<sdk_category>::should_log(CE::Logging::Level::warn);
    CE::Logging::Logger<sdk_category>::set_level_logger(CE::Logging::Level::warn);
    const bool logging = logging_identity && logging_quiet && sdk_logging_accepts_warning();
    MemoryProvider provider;
    CacheBinding::bind_provider(provider);
    const bool cache = sdk_provider_bound(provider);
    bool exception = false;
    try {
        sdk_throw_exception();
    } catch (const CE::Exceptions::runtime_exception& error) {
        exception = std::string_view{error.what()}.find("SDK bridge exception") != std::string_view::npos;
    }
    return singleton && logging && cache && exception ? 0 : 1;
}
