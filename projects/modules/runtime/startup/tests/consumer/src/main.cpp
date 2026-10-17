#include <cheryl/modules/startup.h>
#include <cheryl/core/engine/startup.h>

#include <string>

using Factory = CE::Application::Startup (*)(std::string, CE::Application::ApplicationOptions);
Factory volatile factory = &CE::Application::make_startup;

int main() {
    return factory ? 0 : 1;
}
