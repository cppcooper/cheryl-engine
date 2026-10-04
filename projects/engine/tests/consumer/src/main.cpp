#include <cheryl/core/engine/worker-pool.h>
#include <cheryl/core/subsystems/event-bus.h>
#include <cheryl/core/logging.h>

#include <any>
#include <string_view>

#if defined(GL_VERSION_3_3) || defined(GLFW_VERSION_MAJOR)
#error Generic consumer headers must not include OpenGL or GLFW.
#endif

bool exercise_resources();

int main() {
    CE::SubSystems::EventBus events;
    int observed = 0;
    const auto registration = events.register_listener("consumer", [&](const std::any& value) { observed = std::any_cast<int>(value); });
    events.dispatch("consumer", 7);
    events.unregister_and_wait(registration);

    CE::Engine::WorkerPool workers{1};
    const auto group = workers.make_group();
    auto value = group.submit([] { return 11; });
    group.close();
    group.drain();
    workers.shutdown();
    const bool log_names = std::string_view{CE::enginelog} == "engine" && std::string_view{CE::platformlog} == "os-platform" &&
                           std::string_view{CE::renderlog} == "rendering" && std::string_view{CE::assetlog} == "assets" &&
                           std::string_view{CE::memlog} == "memory" && std::string_view{CE::ce_log_name} == "cheryl";
    return observed == 7 && value.get() == 11 && exercise_resources() && log_names ? 0 : 1;
}
