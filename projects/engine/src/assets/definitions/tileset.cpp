#include <assets/definitions/tileset.h>

namespace CE::Assets::Tilesets {
    std::size_t WangSignatureHash::operator()(const WangSignature& signature) const noexcept {
        std::size_t result = 1469598103934665603ULL;
        for (const auto value : signature) {
            result ^= value;
            result *= 1099511628211ULL;
        }
        return result;
    }

    std::string Definition::id() const {
        return name_space + ':' + name;
    }
}
