#include <backends/miniaudio.h>
#include <internals/exceptions.h>
#include "native-errors.h"

#include <miniaudio.h>

#include <algorithm>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace CE::Audio::Miniaudio::Detail {
    struct VoiceData {
        std::optional<Clip> clip;
        std::optional<std::filesystem::path> file;
        ma_audio_buffer buffer{};
        ma_sound sound{};
        bool buffer_initialized = false;
        bool sound_initialized = false;
        PlaybackState requested = PlaybackState::Paused;
        float volume = 1;
        bool looping = false;
        std::optional<std::chrono::duration<double>> duration;

        void retire() noexcept {
            if (sound_initialized) {
                ma_sound_uninit(&sound);
                sound_initialized = false;
            }
            if (buffer_initialized) {
                ma_audio_buffer_uninit(&buffer);
                buffer_initialized = false;
            }
            clip.reset();
        }

        ~VoiceData() { retire(); }

        [[nodiscard]] PlaybackState state() const {
            if (!sound_initialized)
                return PlaybackState::Closed;
            if (requested == PlaybackState::Stopped)
                return PlaybackState::Stopped;
            if (ma_sound_at_end(&sound))
                return PlaybackState::Finished;
            return requested;
        }
    };

    struct SystemState {
        mutable std::mutex mutex;
        ma_context context{};
        ma_engine engine{};
        bool context_initialized = false;
        bool engine_initialized = false;
        bool offline = false;
        float volume = 1;
        Format output;
        std::string backend;
        std::vector<std::shared_ptr<VoiceData>> voices;

        void require_open() const {
            if (!engine_initialized)
                throw Exceptions::bad_request(CE_HERE, "Audio system is closed");
        }

        void maintain_locked() {
            std::erase_if(voices, [](const auto& voice) {
                return voice.use_count() == 1 && voice->state() != PlaybackState::Playing;
            });
        }

        void close() noexcept {
            const std::lock_guard lock(mutex);
            if (engine_initialized) {
                static_cast<void>(ma_engine_stop(&engine));
                for (const auto& voice : voices)
                    voice->retire();
                voices.clear();
                ma_engine_uninit(&engine);
                engine_initialized = false;
            }
            if (context_initialized) {
                ma_context_uninit(&context);
                context_initialized = false;
            }
        }

        ~SystemState() { close(); }
    };

    [[nodiscard]] std::shared_ptr<VoiceData> make_voice(SystemState& system, const Clip& clip) {
        auto data = std::make_shared<VoiceData>();
        data->clip = clip;
        data->duration = clip.duration();
        auto config = ma_audio_buffer_config_init(
            ma_format_f32, clip.format().channels, clip.frame_count(), clip.samples().data(), nullptr
        );
        config.sampleRate = clip.format().sample_rate;
        check(ma_audio_buffer_init(&config, &data->buffer), "Initialize audio clip cursor");
        data->buffer_initialized = true;
        check(
            ma_sound_init_from_data_source(&system.engine, &data->buffer, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &data->sound),
            "Initialize audio clip voice"
        );
        data->sound_initialized = true;
        return data;
    }

    [[nodiscard]] std::shared_ptr<VoiceData> make_voice(SystemState& system, const std::filesystem::path& file) {
        auto data = std::make_shared<VoiceData>();
        data->file = file;
        constexpr auto flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_WAIT_INIT | MA_SOUND_FLAG_NO_SPATIALIZATION;
        #if defined(_WIN32)
            check(ma_sound_init_from_file_w(&system.engine, file.c_str(), flags, nullptr, nullptr, &data->sound),
                "Open streamed audio voice");
        #else
            check(ma_sound_init_from_file(&system.engine, file.c_str(), flags, nullptr, nullptr, &data->sound),
                "Open streamed audio voice");
        #endif
        data->sound_initialized = true;
        float seconds = 0;
        if (ma_sound_get_length_in_seconds(&data->sound, &seconds) == MA_SUCCESS)
            data->duration = std::chrono::duration<double>(seconds);
        return data;
    }

    class VoiceHandle final : public Voice {
        std::shared_ptr<SystemState> system_;
        std::shared_ptr<VoiceData> data_;

    public:
        VoiceHandle(std::shared_ptr<SystemState> system, std::shared_ptr<VoiceData> data)
        : system_(std::move(system)), data_(std::move(data)) {}

        [[nodiscard]] VoiceSnapshot snapshot() const override {
            const std::lock_guard lock(system_->mutex);
            return {data_->state(), data_->volume, data_->looping, data_->duration};
        }

        void pause() override {
            const std::lock_guard lock(system_->mutex);
            system_->require_open();
            if (data_->state() == PlaybackState::Playing) {
                check(ma_sound_stop(&data_->sound), "Pause audio voice");
                data_->requested = PlaybackState::Paused;
            }
        }

        void resume() override {
            const std::lock_guard lock(system_->mutex);
            system_->require_open();
            if (data_->state() == PlaybackState::Paused) {
                check(ma_sound_start(&data_->sound), "Resume audio voice");
                data_->requested = PlaybackState::Playing;
            }
        }

        void stop() override {
            const std::lock_guard lock(system_->mutex);
            system_->require_open();
            check(ma_sound_stop(&data_->sound), "Stop audio voice");
            data_->requested = PlaybackState::Stopped;
        }

        void restart() override {
            const std::lock_guard lock(system_->mutex);
            system_->require_open();
            // A fresh node/cursor also discards decoder/resampler processing caches.
            // Prepare it before stopping the current voice, so admission failure
            // leaves that voice intact. Native start failure leaves it Stopped.
            auto replacement = data_->clip ? make_voice(*system_, *data_->clip) : make_voice(*system_, *data_->file);
            ma_sound_set_volume(&replacement->sound, data_->volume);
            ma_sound_set_looping(&replacement->sound, data_->looping ? MA_TRUE : MA_FALSE);
            replacement->volume = data_->volume;
            replacement->looping = data_->looping;
            check(ma_sound_stop(&data_->sound), "Stop audio voice for restart");
            data_->requested = PlaybackState::Stopped;
            check(ma_sound_start(&replacement->sound), "Restart audio voice");
            replacement->requested = PlaybackState::Playing;
            const auto slot = std::find(system_->voices.begin(), system_->voices.end(), data_);
            *slot = replacement;
            data_->retire();
            data_ = std::move(replacement);
        }

        void set_volume(float volume) override {
            validate_volume(volume);
            const std::lock_guard lock(system_->mutex);
            system_->require_open();
            ma_sound_set_volume(&data_->sound, volume);
            data_->volume = volume;
        }

        void set_looping(bool looping) override {
            const std::lock_guard lock(system_->mutex);
            system_->require_open();
            ma_sound_set_looping(&data_->sound, looping ? MA_TRUE : MA_FALSE);
            data_->looping = looping;
        }
    };

    [[nodiscard]] std::shared_ptr<SystemState> open(Format output, bool offline) {
        output.validate();
        auto state = std::make_shared<SystemState>();
        state->offline = offline;
        auto config = ma_engine_config_init();
        config.channels = output.channels;
        config.sampleRate = output.sample_rate;
        config.noDevice = offline ? MA_TRUE : MA_FALSE;

        if (!offline) {
            // Try device creation as well as context creation for each real backend.
            // An initialized server context may still have no usable output device.
            ma_result last_result = MA_NO_BACKEND;
            for (int candidate = 0; candidate < static_cast<int>(ma_backend_null); ++candidate) {
                const auto backend = static_cast<ma_backend>(candidate);
                if (backend == ma_backend_custom || !ma_is_backend_enabled(backend))
                    continue;
                const auto context_config = ma_context_config_init();
                last_result = ma_context_init(&backend, 1, &context_config, &state->context);
                if (last_result == MA_OUT_OF_MEMORY)
                    check(last_result, "Initialize native audio context");
                if (last_result != MA_SUCCESS)
                    continue;
                state->context_initialized = true;
                config.pContext = &state->context;
                last_result = ma_engine_init(&config, &state->engine);
                if (last_result == MA_SUCCESS) {
                    state->engine_initialized = true;
                    state->backend = ma_get_backend_name(backend);
                    break;
                }
                if (last_result == MA_OUT_OF_MEMORY)
                    check(last_result, "Initialize native audio output");
                ma_context_uninit(&state->context);
                state->context_initialized = false;
            }
            if (!state->engine_initialized)
                check(last_result, "Open a native audio output device");
        } else {
            state->backend = "offline";
            check(ma_engine_init(&config, &state->engine), "Initialize offline audio output");
            state->engine_initialized = true;
        }

        state->output = {ma_engine_get_channels(&state->engine), ma_engine_get_sample_rate(&state->engine)};
        return state;
    }

    [[nodiscard]] std::shared_ptr<Voice> publish_voice(
        const std::shared_ptr<SystemState>& system,
        const std::shared_ptr<VoiceData>& data,
        PlaybackOptions options
    ) {
        ma_sound_set_volume(&data->sound, options.volume);
        ma_sound_set_looping(&data->sound, options.looping ? MA_TRUE : MA_FALSE);
        data->volume = options.volume;
        data->looping = options.looping;
        auto handle = std::make_shared<VoiceHandle>(system, data);
        system->voices.push_back(data);
        if (!options.paused) {
            try {
                check(ma_sound_start(&data->sound), "Start audio voice");
                data->requested = PlaybackState::Playing;
            } catch (...) {
                system->voices.pop_back();
                throw;
            }
        }
        return handle;
    }
}

namespace CE::Audio::Miniaudio {
    System::System(std::shared_ptr<Detail::SystemState> state) : state_(std::move(state)) {}

    System::~System() { close(); }

    std::unique_ptr<System> System::open_device(Format output) {
        return std::unique_ptr<System>(new System(Detail::open(output, false)));
    }

    std::unique_ptr<System> System::open_offline(Format output) {
        return std::unique_ptr<System>(new System(Detail::open(output, true)));
    }

    std::shared_ptr<Voice> System::play(const Clip& clip, PlaybackOptions options) {
        options.validate();
        const std::lock_guard lock(state_->mutex);
        state_->require_open();
        state_->maintain_locked();
        auto data = Detail::make_voice(*state_, clip);
        return Detail::publish_voice(state_, data, options);
    }

    std::shared_ptr<Voice> System::stream(const std::filesystem::path& file, PlaybackOptions options) {
        options.validate();
        Detail::validate_path(file);
        const std::lock_guard lock(state_->mutex);
        state_->require_open();
        state_->maintain_locked();
        auto data = Detail::make_voice(*state_, file);
        return Detail::publish_voice(state_, data, options);
    }

    void System::set_volume(float volume) {
        validate_volume(volume);
        const std::lock_guard lock(state_->mutex);
        state_->require_open();
        Detail::check(ma_engine_set_volume(&state_->engine, volume), "Set audio master volume");
        state_->volume = volume;
    }

    float System::volume() const {
        const std::lock_guard lock(state_->mutex);
        state_->require_open();
        return state_->volume;
    }

    void System::maintain() {
        const std::lock_guard lock(state_->mutex);
        state_->require_open();
        state_->maintain_locked();
    }

    void System::close() noexcept { state_->close(); }

    bool System::closed() const {
        const std::lock_guard lock(state_->mutex);
        return !state_->engine_initialized;
    }

    Format System::output_format() const {
        const std::lock_guard lock(state_->mutex);
        state_->require_open();
        return state_->output;
    }

    std::string System::backend_name() const {
        const std::lock_guard lock(state_->mutex);
        state_->require_open();
        return state_->backend;
    }

    void System::render(std::span<float> output) {
        const std::lock_guard lock(state_->mutex);
        state_->require_open();
        if (!state_->offline)
            throw Exceptions::bad_request(CE_HERE, "Manual audio mixing requires explicit offline output");
        if (output.size() % state_->output.channels != 0)
            throw Exceptions::invalid_args(CE_HERE, "Offline audio output requires complete interleaved frames");
        if (output.empty())
            return;
        ma_uint64 frames_read = 0;
        const auto frame_count = output.size() / state_->output.channels;
        Detail::check(ma_engine_read_pcm_frames(&state_->engine, output.data(), frame_count, &frames_read), "Mix offline audio");
        if (frames_read != frame_count)
            throw Exceptions::runtime_exception(CE_HERE, "Offline audio mixer returned incomplete output");
    }
}
