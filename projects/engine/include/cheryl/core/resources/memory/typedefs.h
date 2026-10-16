#pragma once
#include <templates/block.h>

namespace CE::Memory {
    // Heap memory, does nothing on losing scope
    using HeapBlock = CE::Memory::Block<void>;
    using ByteBlock = CE::Memory::Block<void>;
    using OptionalByteBlock = CE::Memory::OptionalBlock<void>;

    namespace Ordering {
        using BytePoolOrder = CE::Memory::Ordering::PoolOrder<void>;
        using ByteHeadOrder = CE::Memory::Ordering::HeadOrder<void>;
        using ByteRegistryOrder = CE::Memory::Ordering::RegistryOrder<void>;
    }
}
