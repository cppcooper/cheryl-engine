#include <internals/compile-time-logging.hpp>

constexpr auto mask_before_block = CE::Logging::Compile::compiled_mask;
#include <templates/block.h>

// A separate first-include translation unit catches both header dependencies and
// the former include-order override without redefining policy in a linked program.
static_assert(CE::Logging::Compile::compiled_mask == mask_before_block);
static_assert(CTWriteMask == mask_before_block);
