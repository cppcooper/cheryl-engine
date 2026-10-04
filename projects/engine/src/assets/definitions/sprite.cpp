#include <assets/definitions/sprite.h>

namespace CE::Assets {
    std::string SpriteDefinition::id() const {
        return name_space + ':' + name;
    }
}
