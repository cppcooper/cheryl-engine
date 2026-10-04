// Engine bootstrap: final consumers receive this object through Cheryl::Engine.
// Match the original NDEBUG scope; exception trace capture is independent.
#include <backward.hpp>
#ifndef NDEBUG
backward::SignalHandling sh;
#endif
backward::TraceResolver tr;
