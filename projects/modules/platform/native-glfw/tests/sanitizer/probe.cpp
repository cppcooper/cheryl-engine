#include <cheryl/backends/native-glfw/diagnostics.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>

namespace {
    bool leak_external() {
        // Use the real dependency allocator without needing a compositor or GPU.
        // Retain its loader handle so LeakSanitizer can symbolize the allocation.
        const auto library = dlopen("libfontconfig.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!library)
            return false;
        const auto create = reinterpret_cast<void* (*)()>(dlsym(library, "FcPatternCreate"));
        if (!create)
            return false;
        if (!create())
            std::abort();
        std::fputs("external allocation created\n", stderr);
        return true;
    }

    void leak_application() {
        // The checker excludes stack/register roots so a stale temporary cannot
        // accidentally keep this deliberately abandoned allocation reachable.
        auto* allocation = std::malloc(113);
        if (!allocation)
            std::abort();
        std::memset(allocation, 0x5a, 113);
        std::fprintf(stderr, "application allocation created: %p\n", allocation);
    }
}

int main(const int argc, const char* const* argv) {
    if (argc != 2 || (std::strcmp(argv[1], "external") != 0 && std::strcmp(argv[1], "application") != 0))
        return 2;
    // Exercise the same diagnostics owner pulled into static display consumers.
    CE::Platform::GLFW::Detail::report_glfw_diagnostics("lsan_probe", true);
    if (!leak_external())
        return 77;
    if (std::strcmp(argv[1], "application") == 0)
        leak_application();
    return 0;
}
