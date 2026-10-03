#include <cheryl/core/engine/worker-pool.h>
#include <cheryl/core/subsystems/event-bus.h>

#include <any>

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
    return observed == 7 && value.get() == 11 && exercise_resources() ? 0 : 1;
}
