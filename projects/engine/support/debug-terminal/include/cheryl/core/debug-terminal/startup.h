#pragma once

namespace CE::Application::DebugTerminal {
    using Main = int (*)(int argc, char** argv);
    using Start = void (*)();

    // The module owns its session and shutdown. No callback leaves startup untouched.
    [[nodiscard]] int run(int argc, char** argv, Main application, Start start = nullptr);
}
