#include "audio-demo.h"

#ifdef CHERYL_DEMO_AUDIO
#include <backends/miniaudio.h>
#include <core/engine/engine-context.h>

#include <exception>
#include <optional>
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

void DemoAudio::load(const std::filesystem::path& asset_root) {
#ifdef CHERYL_DEMO_AUDIO
    try {
        audio_state_->system = CE::Audio::Miniaudio::System::open_device();
        audio_state_->system->set_volume(0.35f);
        audio_state_->music = audio_state_->system->stream(
            asset_root / "sfx/Minifantasy_ForgottenPlains_Music/Music/Fair_Fight_(Battle).wav",
            {.volume = 0.5f, .looping = true}
        );
        audio_state_->click = CE::Audio::Miniaudio::decode_file(
            asset_root / "sfx/Helton Yan's Pixel Combat - Single Files/UIClick_INTERFACE-Strong Click 2_HY_PC-004.wav"
        );
        audio_state_->status = "music playing";
    } catch (const std::exception& error) {
        audio_state_->status = std::string("unavailable: ") + error.what();
        audio_state_->music.reset();
        audio_state_->system.reset();
    }
#else
    static_cast<void>(asset_root);
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
