#include <application/startup.h>
#include "startup-internal.h"
#include <internals/exceptions.h>

#include <utility>

namespace CE::Application {
    Startup make_startup(std::string description, ApplicationOptions configuration) {
#if CHERYL_STARTUP_OPENGL
        return Detail::make_opengl_startup(nullptr, std::move(description), std::move(configuration));
#else
        static_cast<void>(description);
        static_cast<void>(configuration);
        throw CE::Exceptions::failed_operation(CE_HERE, "Startup was built without a standard backend; supply StartupBackend");
#endif
    }

    Startup make_startup(CE::Input::System& input, std::string description, ApplicationOptions configuration) {
#if CHERYL_STARTUP_OPENGL
        return Detail::make_opengl_startup(&input, std::move(description), std::move(configuration));
#else
        static_cast<void>(input);
        static_cast<void>(description);
        static_cast<void>(configuration);
        throw CE::Exceptions::failed_operation(CE_HERE, "Startup was built without a standard backend; supply StartupBackend");
#endif
    }
}
