#include <assets/definitions/sprite.h>

namespace CE::Assets::Sprites {
    std::string Definition::id() const {
        return name_space + ':' + name;
    }
}
