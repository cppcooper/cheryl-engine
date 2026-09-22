#pragma once
#include "logging/osink.h"
#include "logging/log.h"
#include "logging/logger.h"

#ifdef ST_ON_SIGNALS
  #include <backward.hpp>
  extern backward::SignalHandling sh;
#endif
