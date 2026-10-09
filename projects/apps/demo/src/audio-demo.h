#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <string_view>

namespace CE::Engine {
    class EngineContext;
}

// Small application-owned audio example: stream music, play a decoded effect,
// and maintain the backend from the game update.
class DemoAudio final {
    struct AudioState;
    std::unique_ptr<AudioState> audio_state_;

public:
    explicit DemoAudio(CE::Engine::EngineContext& engine);
    ~DemoAudio();
    DemoAudio(const DemoAudio&) = delete;
    DemoAudio& operator=(const DemoAudio&) = delete;

    void load(const std::filesystem::path& asset_root);
    void update();
    void play_click();
    [[nodiscard]] std::string_view status() const;
};
