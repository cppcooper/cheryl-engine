#pragma once
#include <templates/block.h>

namespace CE::Mem {
    // Heap memory, does nothing on losing scope
    using HeapBlock = Block<void>;
    using Block = Block<void>;
    using OBlock = OBlock<void>;

    namespace compare {
        using PoolOrder = ::compare::PoolOrder<void>;
        using HeadOrder = ::compare::HeadOrder<void>;
        using RegistryOrder = ::compare::RegistryOrder<void>;
    }
}



