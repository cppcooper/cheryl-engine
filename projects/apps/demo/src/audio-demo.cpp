#include "audio-demo.h"

#ifdef CHERYL_DEMO_AUDIO
#include <backends/miniaudio.h>
#include <core/engine/engine-context.h>
#include <core/resources/asset-management/file-registry.h>

#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#endif

struct DemoAudio::AudioState {
#ifdef CHERYL_DEMO_AUDIO
    std::unique_ptr<CE::Audio::Miniaudio::System> system;
    std::shared_ptr<CE::Audio::Voice> music;
    std::optional<CE::Audio::Clip> click;
    std::string status = "not loaded";

    explicit AudioState(CE::Engine::EngineContext&) {}
#else
    explicit AudioState(CE::Engine::EngineContext&) {}
#endif
};

DemoAudio::DemoAudio(CE::Engine::EngineContext& engine) : audio_state_(std::make_unique<AudioState>(engine)) {}

DemoAudio::~DemoAudio() = default;

void DemoAudio::load() {
#ifdef CHERYL_DEMO_AUDIO
    try {
        auto& registry = CE::Assets::FileRegistry::get();
        const auto music = registry.get_file_named("Fair_Fight_(Battle).wav");
        const auto click = registry.get_file_named("UIClick_INTERFACE-Strong Click 2_HY_PC-004.wav");
        if (!music || !click)
            throw std::runtime_error("The demo music or click effect was not found in the asset tree");
        audio_state_->system = CE::Audio::Miniaudio::System::open_device();
        audio_state_->system->set_volume(0.35f);
        audio_state_->music = audio_state_->system->stream(*music, {.volume = 0.5f, .looping = true});
        audio_state_->click = CE::Audio::Miniaudio::decode_file(*click);
        audio_state_->status = "music playing";
    } catch (const std::exception& error) {
        audio_state_->status = std::string("unavailable: ") + error.what();
        audio_state_->music.reset();
        audio_state_->system.reset();
    }
#endif
}

void DemoAudio::update() {
#ifdef CHERYL_DEMO_AUDIO
    if (audio_state_->system)
        audio_state_->system->maintain();
#endif
}

void DemoAudio::play_click() {
#ifdef CHERYL_DEMO_AUDIO
    if (audio_state_->system && audio_state_->click)
        static_cast<void>(audio_state_->system->play(*audio_state_->click, {.volume = 0.45f}));
#endif
}

std::string_view DemoAudio::status() const {
#ifdef CHERYL_DEMO_AUDIO
    return audio_state_->status;
#else
    return "disabled (enable CHERYL_BUILD_AUDIO_MINIAUDIO)";
#endif
}
