#pragma once

#include <chrono>
#include <memory>
#include <string_view>

namespace CE::Runtime {
    class Context;
}

// Small application-owned audio example: stream music, play a decoded effect,
// and maintain the backend from the game update.
class DemoAudio final {
    struct AudioState;
    std::unique_ptr<AudioState> audio_state_;

public:
    explicit DemoAudio(CE::Runtime::Context& engine);
    ~DemoAudio();
    DemoAudio(const DemoAudio&) = delete;
    DemoAudio& operator=(const DemoAudio&) = delete;

    void load();
    void update();
    void play_click();
    [[nodiscard]] std::string_view status() const;
};
