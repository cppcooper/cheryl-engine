#ifdef NDEBUG
  #include <internals/celog.h>
#else
  #define ST_ON_SIGNALS
  #include <internals.h>
  backward::SignalHandling sh;
#endif
//#include <cheryl.h>
backward::TraceResolver tr;
#include <core.h>

