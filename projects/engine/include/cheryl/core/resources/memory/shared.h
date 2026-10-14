#pragma once

#include <memory>

namespace CE::Memory {
    template <typename T> using Shared = std::shared_ptr<T>;
}
