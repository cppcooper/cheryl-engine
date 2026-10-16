// Engine bootstrap: shared Engine owns it; static consumers receive the object.
// Match the original NDEBUG scope; exception trace capture is independent.
#include <cheryl/export.h>
#include <backward.hpp>
#ifndef NDEBUG
CE_API backward::SignalHandling sh;
#endif
CE_API backward::TraceResolver tr;
