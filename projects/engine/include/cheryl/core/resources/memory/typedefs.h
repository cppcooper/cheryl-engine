#pragma once
#include <templates/block.h>

namespace CE::Memory {
    /** Byte-range metadata retaining its backing owner; dropping a range is not a pool return. */
    using HeapBlock = CE::Memory::Block<void>;
    /** Byte-counted Block<void> range, with the same ownership/pool-return distinction as HeapBlock. */
    using ByteBlock = CE::Memory::Block<void>;
    /** Absent/present byte-range result; absence means no compatible range was found/returned. */
    using OptionalByteBlock = CE::Memory::OptionalBlock<void>;

    namespace Ordering {
        /** Alignment/length-first free-byte-range ordering. */
        using BytePoolOrder = CE::Memory::Ordering::PoolOrder<void>;
        /** Address-first byte-section ordering. */
        using ByteHeadOrder = CE::Memory::Ordering::HeadOrder<void>;
        /** Owner-first byte-allocation ordering. */
        using ByteRegistryOrder = CE::Memory::Ordering::RegistryOrder<void>;
    }
}
