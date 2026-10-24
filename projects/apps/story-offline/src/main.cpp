#include "story.h"
#include "continuation.h"
#include "bargain.h"
#include "selection.h"
#include "payoff.h"

#include <charconv>

#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    try {
        if (argc >= 2 && std::string_view(argv[1]) == "--payoff") {
            if (argc == 5 || argc == 6) {
                const std::string_view text = argv[2];
                std::uint64_t seed = 0;
                const auto parsed = std::from_chars(text.data(), text.data() + text.size(), seed);
                if (!text.empty() && parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()) {
                    const auto code = OfflineStory::run_payoff(std::cout, seed, argv[3], argv[4], argc == 6 ? argv[5] : "none");
                    if (code != 2)
                        return std::cout ? code : 3;
                }
            }
            std::cerr
                << "Usage: story-offline --payoff SEED immediate|pay|promise|negotiate|refuse deliver|break|acknowledge|leave [none|restore|drain|absent|bank]\n";
            return 2;
        }
        if (argc >= 2 && std::string_view(argv[1]) == "--select-bargain") {
            if (argc == 5) {
                const std::string_view text = argv[2];
                std::uint64_t seed = 0;
                const auto parsed = std::from_chars(text.data(), text.data() + text.size(), seed);
                if (!text.empty() && parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()) {
                    const auto code = OfflineStory::run_selected(std::cout, seed, argv[3], argv[4]);
                    if (code != 2)
                        return std::cout ? code : 3;
                }
            }
            std::cerr
                << "Usage: story-offline --select-bargain SEED any|open|small|consult|empty|budget|nodraw immediate|promise|negotiate|refuse\n";
            return 2;
        }
        if (argc >= 2 && std::string_view(argv[1]) == "--bargain") {
            if (argc >= 5 && argc <= 7) {
                const std::string_view variant = argc >= 6 ? argv[5] : "0";
                if (variant == "0" || variant == "1") {
                    const int code =
                        OfflineStory::run_bargain(std::cout, argv[2], argv[3], argv[4], variant == "1", argc == 7 ? argv[6] : "none");
                    if (code != 2)
                        return std::cout ? code : 3;
                }
            }
            std::cerr
                << "Usage: story-offline --bargain ramp|shelter pay|repair|decline immediate|pay|promise|negotiate|refuse [0|1 [none|restore|drain]]\n";
            return 2;
        }
        if (argc >= 2 && std::string_view(argv[1]) == "--watch") {
            if (argc >= 5 && argc <= 7) {
                const std::string_view variant = argc >= 6 ? argv[5] : "0";
                if (variant == "0" || variant == "1") {
                    const int code =
                        OfflineStory::run_watch(std::cout, argv[2], argv[3], argv[4], variant == "1", argc == 7 ? argv[6] : "none");
                    if (code != 2)
                        return std::cout ? code : 3;
                }
            }
            std::cerr << "Usage: story-offline --watch tide|storm pay|repair|decline evidence|withhold [0|1 [none|restore|drain]]\n";
            return 2;
        }
        if (argc == 1) {
            for (const auto set : {"tide", "storm"}) {
                for (const auto action : {"pay", "repair", "decline"})
                    if (const int code = OfflineStory::run(std::cout, set, action, 0, "restore"))
                        return code;
                if (const int code = OfflineStory::run(std::cout, set, "pay", 1, "none"))
                    return code;
            }
            return std::cout ? 0 : 3;
        }
        if (argc >= 3 && argc <= 5) {
            const std::string_view variant = argc >= 4 ? argv[3] : "0";
            if (variant == "0" || variant == "1") {
                const int code = OfflineStory::run(std::cout, argv[1], argv[2], variant == "1", argc == 5 ? argv[4] : "none");
                if (code != 2)
                    return std::cout ? code : 3;
            }
        }
        std::cerr << "Usage: story-offline [tide|storm pay|repair|decline [0|1 [none|restore|drain]]]\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "Offline encounter failed: " << error.what() << '\n';
        return 3;
    }
}
