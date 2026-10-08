#include <cheryl/backends/miniaudio.h>

#if defined(miniaudio_h) || defined(GLFW_VERSION_MAJOR) || defined(GL_VERSION_3_3)
    #error Audio consumers must not inherit native audio or graphics SDK headers.
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

int main() {
    try {
        auto system = CE::Audio::Miniaudio::System::open_offline();
        const auto clip = CE::Audio::Clip::from_samples({2, 48000}, std::vector<float>(2048, 0.25f));
        auto voice = system->play(clip, {.volume = 0.5f});
        std::array<float, 128> output{};
        system->render(output);
        if (!std::ranges::all_of(output, [](float sample) { return std::abs(sample - 0.125f) < 0.00001f; }))
            return 1;
        system->close();
        if (voice->snapshot().state != CE::Audio::PlaybackState::Closed)
            return 1;
        std::cout << "Audio consumer: offline PCM and closed-handle checks completed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
