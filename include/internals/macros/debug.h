#pragma once

#ifdef _MSC_VER
  #define CE_FUNCTION_ __FUNCTION__
#else
  #define CE_FUNCTION_ __PRETTY_FUNCTION__
#endif

#define CE_HERE CE_FUNCTION_, __LINE__
#define LOG_HERE std::format("[{}:{}] ", __FILE__, __LINE__)
#define DEBUG_FUNCTION_HERE std::format("{} [{}:{}] ", CE_FUNCTION_, __FILE__, __LINE__)


