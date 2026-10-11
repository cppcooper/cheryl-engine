#pragma once

namespace CE::DebugTerminal {
    using ApplicationMain = int (*)(int argc, char** argv);
    using Start = void (*)();

    // The module owns its session and shutdown. No callback leaves startup untouched.
    [[nodiscard]] int run(int argc, char** argv, ApplicationMain application, Start start = nullptr);
}
