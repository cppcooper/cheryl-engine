// Engine bootstrap: shared Engine owns it; static consumers receive the object.
// Match the original NDEBUG scope; exception trace capture is independent.
#include <cheryl/export.h>
#include <backward.hpp>
namespace {
#ifndef NDEBUG
    backward::SignalHandling signals;
#endif
    backward::TraceResolver traces;
}
