#include <backends/miniaudio.h>
#include <internals/exceptions.h>
#include "native-errors.h"

#include <miniaudio.h>

#include <array>
#include <utility>
#include <vector>

namespace CE::Audio::Miniaudio {
    namespace {
        struct Decoder {
            ma_decoder value{};
            bool initialized = false;

            ~Decoder() {
                if (initialized)
                    static_cast<void>(ma_decoder_uninit(&value));
            }
        };
    }

    Clip decode_file(const std::filesystem::path& file, DecodeOptions options) {
        Detail::validate_path(file);
        if (options.maximum_bytes < sizeof(float))
            throw Exceptions::invalid_args(CE_HERE, "Audio decode budget must hold at least one float sample");

        Decoder decoder;
        const auto config = ma_decoder_config_init(ma_format_f32, 0, 0);
        #if defined(_WIN32)
            Detail::check(ma_decoder_init_file_w(file.c_str(), &config, &decoder.value), "Open audio decoder");
        #else
            Detail::check(ma_decoder_init_file(file.c_str(), &config, &decoder.value), "Open audio decoder");
        #endif
        decoder.initialized = true;
        Format format;
        ma_format native_format{};
        Detail::check(ma_decoder_get_data_format(&decoder.value, &native_format, &format.channels, &format.sample_rate, nullptr, 0),
            "Read audio format");
        format.validate();
        if (native_format != ma_format_f32)
            throw Exceptions::runtime_exception(CE_HERE, "Audio decoder did not provide float PCM");

        const auto maximum_samples = options.maximum_bytes / sizeof(float);
        std::vector<float> samples;
        std::array<float, 4096 * 2> block{};
        for (;;) {
            ma_uint64 frames = 0;
            const auto result = ma_decoder_read_pcm_frames(&decoder.value, block.data(), 4096, &frames);
            if (result != MA_SUCCESS && result != MA_AT_END)
                Detail::check(result, "Decode audio frames");
            const auto count = static_cast<std::size_t>(frames) * format.channels;
            if (count > maximum_samples - samples.size())
                throw Exceptions::runtime_exception(CE_HERE, "Decoded audio exceeds the selected byte budget");
            samples.insert(samples.end(), block.data(), block.data() + count);
            if (result == MA_AT_END || frames == 0)
                break;
        }
        return Clip::from_samples(format, std::move(samples));
    }
}
