#include <csignal>
#include <cstdio>
#include <string_view>

// No engine entry point is referenced: only the Engine target's link contract
// can deliver its otherwise unreferenced automatic bootstrap to this consumer.
int main(int argc, char** argv) {
    if (argc != 2)
        return 2;
    const std::string_view scenario(argv[1]);
    if (scenario == "scope") {
#ifdef NDEBUG
        std::puts("ndebug=1");
#else
        std::puts("ndebug=0");
#endif
        return 0;
    }
    if (scenario == "crash") {
        std::fputs("raising SIGABRT\n", stderr);
        std::fflush(stderr);
        std::raise(SIGABRT);
        return 3;
    }
    return 2;
}
