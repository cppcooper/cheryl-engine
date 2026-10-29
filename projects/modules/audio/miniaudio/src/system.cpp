#include <backends/miniaudio.h>
#include <internals/exceptions.h>
#include <internals/compile-time-logging.hpp>
#include <core/diagnostics.h>
#include "native-errors.h"
#include "system-internal.h"

#include <miniaudio.h>

#include <algorithm>
#include <array>
#include <limits>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace CE::Audio::Miniaudio::Detail {
    [[nodiscard]] std::uint64_t identity() {
        const auto id = CE::Diagnostics::next_domain_id();
        if (!id)
            throw CE::Exceptions::failed_operation(CE_HERE, "Playback identity space is exhausted");
        return id;
    }

    struct VoiceData {
        std::optional<::CE::Audio::Clip> clip;
        std::optional<std::filesystem::path> file;
        ma_audio_buffer buffer{};
        ma_resource_manager_data_stream stream{};
        ma_async_notification_event stream_notification{};
        ma_sound sound{};
        bool buffer_initialized = false;
        bool stream_initialized = false;
        bool stream_notification_initialized = false;
        bool sound_initialized = false;
        ::CE::Audio::PlaybackState requested = ::CE::Audio::PlaybackState::Paused;
        float volume = 1;
        bool looping = false;
        bool variable_rate = false;
        float playback_rate = 1;
        std::optional<std::chrono::duration<double>> duration;
        std::uint64_t voice = 0;
        std::uint64_t generation = 0;
        bool admitted = false;
        std::optional<PlaybackTerminal> terminal;

        void retire() noexcept {
            if (sound_initialized) {
                ma_sound_uninit(&sound);
                sound_initialized = false;
            }
            if (stream_initialized) {
                static_cast<void>(ma_resource_manager_data_stream_uninit(&stream));
                stream_initialized = false;
            }
            if (stream_notification_initialized) {
                static_cast<void>(ma_async_notification_event_uninit(&stream_notification));
                stream_notification_initialized = false;
            }
            if (buffer_initialized) {
                ma_audio_buffer_uninit(&buffer);
                buffer_initialized = false;
            }
            clip.reset();
        }

        ~VoiceData() { retire(); }

        [[nodiscard]] ::CE::Audio::PlaybackState state() const {
            if (!sound_initialized)
                return ::CE::Audio::PlaybackState::Closed;
            if (requested == ::CE::Audio::PlaybackState::Stopped)
                return ::CE::Audio::PlaybackState::Stopped;
            if ((terminal && terminal->reason == PlaybackEndReason::Completed) || ma_sound_at_end(&sound))
                return ::CE::Audio::PlaybackState::Finished;
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
        ::CE::Audio::Format output;
        std::string backend;
        std::vector<std::shared_ptr<VoiceData>> voices;
        const std::uint64_t id = identity();
        std::uint64_t sequence = 0;
        std::uint64_t active_generations = 0;
        std::uint64_t lost = 0;
        std::uint64_t first_lost = 0;
        std::uint64_t last_lost = 0;
        std::uint64_t reported_lost = 0;
        std::vector<PlaybackEnded> outbox;
        std::size_t head = 0;
        std::size_t pending = 0;
        CE::Events::EventBus::Publisher publisher;
        std::thread::id publication_owner;
        bool publishing = false;
        bool discarded = false;
        ma_result next_stop = MA_SUCCESS;
        ma_result next_start = MA_SUCCESS;

        explicit SystemState(PlaybackNotificationOptions options) : outbox(options.capacity) {
            if (outbox.empty())
                throw CE::Exceptions::invalid_args(CE_HERE, "Playback notification capacity must be positive");
        }

        struct TerminalPermit {
            SystemState& system;
            bool held = true;
            explicit TerminalPermit(SystemState& state) : system(state) {
                if (system.active_generations >= std::numeric_limits<std::uint64_t>::max() - system.sequence)
                    throw CE::Exceptions::failed_operation(CE_HERE, "Playback terminal sequence space is exhausted");
                ++system.active_generations;
            }
            ~TerminalPermit() {
                if (held)
                    --system.active_generations;
            }
            void admit(VoiceData& data) noexcept {
                data.admitted = true;
                held = false;
            }
        };

        void terminal(VoiceData& data, PlaybackEndReason reason) noexcept {
            if (!data.admitted || data.terminal)
                return;
            data.terminal = PlaybackTerminal{reason, ++sequence, std::chrono::steady_clock::now()};
            --active_generations;
            if (discarded)
                return;
            if (pending == outbox.size()) {
                const auto dropped = outbox[head].terminal.sequence;
                if (!first_lost)
                    first_lost = dropped;
                last_lost = dropped;
                ++lost;
                head = (head + 1) % outbox.size();
                --pending;
            }
            outbox[(head + pending++) % outbox.size()] = {id, data.voice, data.generation, *data.terminal};
        }

        void observe(VoiceData& data) noexcept {
            if (data.sound_initialized && !data.terminal && ma_sound_at_end(&data.sound))
                terminal(data, PlaybackEndReason::Completed);
        }

        void require_publication_owner() {
            if (publication_owner == std::thread::id{})
                publication_owner = std::this_thread::get_id();
            if (publication_owner != std::this_thread::get_id() || publishing)
                throw CE::Exceptions::bad_request(CE_HERE, "Playback publication requires its designated owner and cannot recurse");
        }

        [[nodiscard]] PlaybackNotifications snapshot_locked() const noexcept {
            return {id, sequence, lost, first_lost, last_lost, pending, outbox.size(), !engine_initialized};
        }

        void require_open() const {
            if (!engine_initialized)
                throw ::CE::Exceptions::bad_request(CE_HERE, "Audio system is closed");
        }

        void stop(VoiceData& data, const char* operation) {
            check(std::exchange(next_stop, MA_SUCCESS), operation);
            check(ma_sound_stop(&data.sound), operation);
        }
        void start(VoiceData& data, const char* operation) {
            check(std::exchange(next_start, MA_SUCCESS), operation);
            check(ma_sound_start(&data.sound), operation);
        }

        void maintain_locked() {
            std::erase_if(voices, [this](const auto& voice) {
                observe(*voice);
                if (voice.use_count() == 1 && voice->state() != ::CE::Audio::PlaybackState::Playing) {
                    terminal(*voice, PlaybackEndReason::Reclaimed);
                    return true;
                }
                return false;
            });
        }

        void close() noexcept {
            const std::lock_guard lock(mutex);
            if (engine_initialized) {
                static_cast<void>(ma_engine_stop(&engine));
                for (const auto& voice : voices) {
                    observe(*voice);
                    terminal(*voice, PlaybackEndReason::OutputClosed);
                    voice->retire();
                }
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

    void publish_notifications(std::shared_ptr<SystemState> state) {
        CE::Events::EventBus::Publisher publisher;
        std::uint64_t cutoff;
        {
            const std::lock_guard lock(state->mutex);
            if (state->discarded)
                return;
            state->require_publication_owner();
            for (const auto& voice : state->voices)
                state->observe(*voice);
            cutoff = state->sequence;
            publisher = state->publisher;
            state->publishing = true;
        }
        // Each record is removed before its attempt. Reentrant producers can stage
        // more outcomes, but cannot extend this pump's immutable entry cutoff.
        std::size_t attempted = 0;
        std::array<std::size_t, 5> outcomes{};
        bool report_loss = false;
        PlaybackNotifications final;
        for (;;) {
            PlaybackEnded record;
            {
                const std::lock_guard lock(state->mutex);
                if (state->discarded || !state->pending || state->outbox[state->head].terminal.sequence > cutoff) {
                    state->publishing = false;
                    final = state->snapshot_locked();
                    report_loss = final.lost_observations != state->reported_lost;
                    state->reported_lost = final.lost_observations;
                    break;
                }
                record = state->outbox[state->head];
                state->head = (state->head + 1) % state->outbox.size();
                --state->pending;
                record.lost_observations = state->lost;
                record.first_lost_sequence = state->first_lost;
                record.last_lost_sequence = state->last_lost;
            }
            publisher.notify(CE::Audio::playback_ended_event, record);
            ++outcomes[static_cast<std::size_t>(record.terminal.reason)];
            ++attempted;
        }
        if (attempted || report_loss)
            CE_LOG_DEBUG(
                CE::Logging::Categories::engine,
                "subsystem=audio event=playback-ended operation=notification_attempt system={} cutoff={} attempted={} pending={} lost={} first_lost={} last_lost={} closed={} completed={} stopped={} replaced={} output_closed={} reclaimed={}",
                final.system, cutoff, attempted, final.pending, final.lost_observations, final.first_lost_sequence,
                final.last_lost_sequence, final.closed, outcomes[0], outcomes[1], outcomes[2], outcomes[3], outcomes[4]
            );
    }

    [[nodiscard]] std::shared_ptr<VoiceData> make_voice(SystemState& system, const ::CE::Audio::Clip& clip, bool variable_rate) {
        auto data = std::make_shared<VoiceData>();
        data->variable_rate = variable_rate;
        data->clip = clip;
        data->duration = clip.duration();
        auto config = ma_audio_buffer_config_init(
            ma_format_f32, clip.format().channels, clip.frame_count(), clip.samples().data(), nullptr
        );
        config.sampleRate = clip.format().sample_rate;
        check(ma_audio_buffer_init(&config, &data->buffer), "Initialize audio clip cursor");
        data->buffer_initialized = true;
        // Opt into the pitch resampler only for voices that need variable speed.
        const auto flags = MA_SOUND_FLAG_NO_SPATIALIZATION | (variable_rate ? 0 : MA_SOUND_FLAG_NO_PITCH);
        check(
            ma_sound_init_from_data_source(&system.engine, &data->buffer, flags, nullptr, &data->sound),
            "Initialize audio clip voice"
        );
        data->sound_initialized = true;
        return data;
    }

    [[nodiscard]] std::shared_ptr<VoiceData> make_voice(SystemState& system, const std::filesystem::path& file, bool variable_rate) {
        auto data = std::make_shared<VoiceData>();
        data->variable_rate = variable_rate;
        data->file = file;
        auto* manager = ma_engine_get_resource_manager(&system.engine);
        const auto flags = MA_SOUND_FLAG_NO_SPATIALIZATION | (variable_rate ? 0 : MA_SOUND_FLAG_NO_PITCH);
        if ((manager->config.flags & MA_RESOURCE_MANAGER_FLAG_NO_THREADING) != 0) {
            // Without a worker, synchronous initialization finishes the load job
            // before returning and needs no thread-backed notification event.
            const auto stream_flags = flags | MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_WAIT_INIT;
#if defined(_WIN32)
            check(
                ma_sound_init_from_file_w(&system.engine, file.c_str(), stream_flags, nullptr, nullptr, &data->sound),
                "Open streamed audio voice"
            );
#else
            check(
                ma_sound_init_from_file(&system.engine, file.c_str(), stream_flags, nullptr, nullptr, &data->sound),
                "Open streamed audio voice"
            );
#endif
        } else {
            check(ma_async_notification_event_init(&data->stream_notification), "Initialize streamed audio notification");
            data->stream_notification_initialized = true;
            auto notifications = ma_resource_manager_pipeline_notifications_init();
            notifications.init.pNotification = &data->stream_notification;
            auto config = ma_resource_manager_data_source_config_init();
#if defined(_WIN32)
            config.pFilePathW = file.c_str();
#else
            config.pFilePath = file.c_str();
#endif
            config.flags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM | MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC;
            config.pNotifications = &notifications;
            check(ma_resource_manager_data_stream_init_ex(manager, &config, &data->stream), "Queue streamed audio initialization");
            data->stream_initialized = true;
            // The init notification precedes the load job's final access. Keep both
            // objects alive until ordered stream uninitialization joins that work.
            check(ma_async_notification_event_wait(&data->stream_notification), "Wait for streamed audio initialization");
            const auto result = ma_resource_manager_data_stream_result(&data->stream);
            if (result == MA_OUT_OF_MEMORY && data->stream.isDecoderInitialized && data->stream.pPageData == nullptr) {
                // miniaudio already uninitializes the decoder when page allocation
                // fails, but leaves this flag set. Ordered cleanup must not repeat it.
                data->stream.isDecoderInitialized = MA_FALSE;
            }
            check(result, "Open streamed audio voice");
            check(
                ma_sound_init_from_data_source(&system.engine, &data->stream, flags, nullptr, &data->sound),
                "Initialize streamed audio voice"
            );
        }
        data->sound_initialized = true;
        float seconds = 0;
        if (ma_sound_get_length_in_seconds(&data->sound, &seconds) == MA_SUCCESS)
            data->duration = std::chrono::duration<double>(seconds);
        return data;
    }

    class VoiceHandle final : public ::CE::Audio::Voice {
        std::shared_ptr<SystemState> system_;
        std::shared_ptr<VoiceData> data_;

    public:
        VoiceHandle(std::shared_ptr<SystemState> system, std::shared_ptr<VoiceData> data)
        : system_(std::move(system)), data_(std::move(data)) {}

        [[nodiscard]] ::CE::Audio::VoiceSnapshot snapshot() const override {
            const std::lock_guard lock(system_->mutex);
            system_->observe(*data_);
            return {data_->state(), data_->volume, data_->looping, data_->duration,
                    system_->id, data_->voice, data_->generation, data_->terminal, data_->playback_rate};
        }

        void pause() override {
            const std::lock_guard lock(system_->mutex);
            system_->require_open();
            system_->observe(*data_);
            if (data_->state() == ::CE::Audio::PlaybackState::Playing) {
                system_->stop(*data_, "Pause audio voice");
                data_->requested = ::CE::Audio::PlaybackState::Paused;
            }
        }

        void resume() override {
            const std::lock_guard lock(system_->mutex);
            system_->require_open();
            system_->observe(*data_);
            if (data_->state() == ::CE::Audio::PlaybackState::Paused) {
                system_->start(*data_, "Resume audio voice");
                data_->requested = ::CE::Audio::PlaybackState::Playing;
            }
        }

        void stop() override {
            const std::lock_guard lock(system_->mutex);
            system_->require_open();
            system_->observe(*data_);
            system_->stop(*data_, "Stop audio voice");
            data_->requested = ::CE::Audio::PlaybackState::Stopped;
            system_->terminal(*data_, PlaybackEndReason::Stopped);
        }

        void restart() override {
            const std::lock_guard lock(system_->mutex);
            system_->require_open();
            system_->observe(*data_);
            if (data_->generation == std::numeric_limits<std::uint64_t>::max())
                throw CE::Exceptions::failed_operation(CE_HERE, "Playback generation space is exhausted");
            SystemState::TerminalPermit permit(*system_);
            // A fresh node/cursor also discards decoder/resampler processing caches.
            // Prepare it before stopping the current voice, so admission failure
            // leaves that voice intact. Native start failure leaves it Stopped.
            auto replacement = data_->clip
                ? make_voice(*system_, *data_->clip, data_->variable_rate)
                : make_voice(*system_, *data_->file, data_->variable_rate);
            replacement->voice = data_->voice;
            replacement->generation = data_->generation + 1;
            ma_sound_set_volume(&replacement->sound, data_->volume);
            ma_sound_set_looping(&replacement->sound, data_->looping ? MA_TRUE : MA_FALSE);
            replacement->volume = data_->volume;
            replacement->looping = data_->looping;
            if (data_->variable_rate) ma_sound_set_pitch(&replacement->sound, data_->playback_rate);
            replacement->playback_rate = data_->playback_rate;
            system_->stop(*data_, "Stop audio voice for restart");
            data_->requested = ::CE::Audio::PlaybackState::Stopped;
            try {
                system_->start(*replacement, "Restart audio voice");
            } catch (...) {
                system_->terminal(*data_, PlaybackEndReason::Stopped);
                throw;
            }
            replacement->requested = ::CE::Audio::PlaybackState::Playing;
            system_->terminal(*data_, PlaybackEndReason::Replaced);
            permit.admit(*replacement);
            const auto slot = std::find(system_->voices.begin(), system_->voices.end(), data_);
            *slot = replacement;
            data_->retire();
            data_ = std::move(replacement);
        }

        void set_volume(float volume) override {
            ::CE::Audio::validate_volume(volume);
            const std::lock_guard lock(system_->mutex);
            system_->require_open();
            system_->observe(*data_);
            ma_sound_set_volume(&data_->sound, volume);
            data_->volume = volume;
        }

        void set_playback_rate(float rate) override {
            ::CE::Audio::validate_playback_rate(rate);
            const std::lock_guard lock(system_->mutex);
            system_->require_open();
            if (!data_->variable_rate)
                throw CE::Exceptions::bad_request(CE_HERE, "Create playback with variable_rate to control its speed");
            system_->observe(*data_);
            ma_sound_set_pitch(&data_->sound, rate);
            data_->playback_rate = rate;
        }

        void set_looping(bool looping) override {
            const std::lock_guard lock(system_->mutex);
            system_->require_open();
            system_->observe(*data_);
            if (data_->terminal && data_->terminal->reason == PlaybackEndReason::Completed)
                system_->stop(*data_, "Preserve completed audio voice");
            ma_sound_set_looping(&data_->sound, looping ? MA_TRUE : MA_FALSE);
            data_->looping = looping;
        }
    };

    [[nodiscard]] std::shared_ptr<SystemState> open(::CE::Audio::Format output, bool offline, PlaybackNotificationOptions notifications) {
        output.validate();
        auto state = std::make_shared<SystemState>(notifications);
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

    [[nodiscard]] std::shared_ptr<::CE::Audio::Voice> publish_voice(
        const std::shared_ptr<SystemState>& system,
        const std::shared_ptr<VoiceData>& data,
        ::CE::Audio::PlaybackOptions options,
        std::uint64_t voice,
        SystemState::TerminalPermit& permit
    ) {
        data->voice = voice;
        data->generation = 1;
        ma_sound_set_volume(&data->sound, options.volume);
        ma_sound_set_looping(&data->sound, options.looping ? MA_TRUE : MA_FALSE);
        data->volume = options.volume;
        data->looping = options.looping;
        auto handle = std::make_shared<VoiceHandle>(system, data);
        system->voices.push_back(data);
        if (!options.paused) {
            try {
                system->start(*data, "Start audio voice");
                data->requested = ::CE::Audio::PlaybackState::Playing;
            } catch (...) {
                system->voices.pop_back();
                throw;
            }
        }
        permit.admit(*data);
        return handle;
    }

    void SystemAccess::fail_next_stop(System& system) {
        const std::lock_guard lock(system.state_->mutex);
        system.state_->next_stop = MA_ERROR;
    }
    void SystemAccess::fail_next_start(System& system) {
        const std::lock_guard lock(system.state_->mutex);
        system.state_->next_start = MA_ERROR;
    }
    void SystemAccess::reserve_last_terminal(System& system) {
        const std::lock_guard lock(system.state_->mutex);
        if (system.state_->active_generations || system.state_->sequence)
            throw CE::Exceptions::bad_request(CE_HERE, "Counter fixture requires a fresh output");
        system.state_->sequence = std::numeric_limits<std::uint64_t>::max() - 1;
    }
}

namespace CE::Audio::Miniaudio {
    System::System(std::shared_ptr<Detail::SystemState> state) : state_(std::move(state)) {}

    System::~System() {
        close();
        const std::lock_guard lock(state_->mutex);
        state_->discarded = true;
        state_->pending = 0;
    }

    std::unique_ptr<System> System::open_device(::CE::Audio::Format output, PlaybackNotificationOptions notifications) {
        return std::unique_ptr<System>(new System(Detail::open(output, false, notifications)));
    }

    std::unique_ptr<System> System::open_offline(::CE::Audio::Format output, PlaybackNotificationOptions notifications) {
        return std::unique_ptr<System>(new System(Detail::open(output, true, notifications)));
    }

    std::shared_ptr<::CE::Audio::Voice> System::play(const ::CE::Audio::Clip& clip, ::CE::Audio::PlaybackOptions options) {
        options.validate();
        const std::lock_guard lock(state_->mutex);
        state_->require_open();
        state_->maintain_locked();
        Detail::SystemState::TerminalPermit permit(*state_);
        const auto id = Detail::identity();
        auto data = Detail::make_voice(*state_, clip, options.variable_rate);
        return Detail::publish_voice(state_, data, options, id, permit);
    }

    std::shared_ptr<::CE::Audio::Voice> System::stream(const std::filesystem::path& file, ::CE::Audio::PlaybackOptions options) {
        options.validate();
        Detail::validate_path(file);
        const std::lock_guard lock(state_->mutex);
        state_->require_open();
        state_->maintain_locked();
        Detail::SystemState::TerminalPermit permit(*state_);
        const auto id = Detail::identity();
        auto data = Detail::make_voice(*state_, file, options.variable_rate);
        return Detail::publish_voice(state_, data, options, id, permit);
    }

    void System::set_volume(float volume) {
        ::CE::Audio::validate_volume(volume);
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

    void System::publish_events_to(CE::Events::EventBus::Publisher publisher) {
        const std::lock_guard lock(state_->mutex);
        state_->require_publication_owner();
        state_->publisher = std::move(publisher);
    }

    void System::publish_notifications() { Detail::publish_notifications(state_); }

    PlaybackNotifications System::notification_snapshot() const {
        const std::lock_guard lock(state_->mutex);
        return state_->snapshot_locked();
    }

    CE::Runtime::ServiceAttachment System::pump_with(const CE::Runtime::RuntimeServices& services) {
        {
            const std::lock_guard lock(state_->mutex);
            state_->require_publication_owner();
        }
        return services.on_maintenance([weak = std::weak_ptr(state_)](const CE::Runtime::PlatformMaintenance&) {
            if (auto state = weak.lock())
                Detail::publish_notifications(std::move(state));
        });
    }

    ::CE::Audio::Format System::output_format() const {
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
            throw ::CE::Exceptions::bad_request(CE_HERE, "Manual audio mixing requires explicit offline output");
        if (output.size() % state_->output.channels != 0)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Offline audio output requires complete interleaved frames");
        if (output.empty())
            return;
        ma_uint64 frames_read = 0;
        const auto frame_count = output.size() / state_->output.channels;
        const auto first_frame = ma_engine_get_time_in_pcm_frames(&state_->engine);
        Detail::check(ma_engine_read_pcm_frames(&state_->engine, output.data(), frame_count, &frames_read), "Mix offline audio");
        if (frames_read > frame_count)
            throw ::CE::Exceptions::runtime_exception(CE_HERE, "Offline audio mixer exceeded the requested frame count");
        if (frames_read < frame_count) {
            // A graph without active inputs returns a successful short read. Silence
            // fills the requested interval and still advances the offline clock.
            std::ranges::fill(output.subspan(static_cast<std::size_t>(frames_read) * state_->output.channels), 0.0f);
            Detail::check(ma_engine_set_time_in_pcm_frames(&state_->engine, first_frame + frame_count), "Advance offline audio clock");
        }
    }
}
