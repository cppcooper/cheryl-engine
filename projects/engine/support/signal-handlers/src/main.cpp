// Optional application-owned bootstrap, linked explicitly with Cheryl::SignalHandlers.
// The engine archive and ordinary consumers never extract this object incidentally.
#include <backward.hpp>
backward::SignalHandling sh;
backward::TraceResolver tr;
