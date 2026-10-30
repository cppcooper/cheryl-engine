#pragma once

#if defined(_WIN32)
#if !defined(NTDDI_VERSION)
#define NTDDI_VERSION 0x0A000007
#elif NTDDI_VERSION < 0x0A000007
#error Cheryl requires Windows 10 version 1903 or later
#endif
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0A00
#error Cheryl requires Windows 10 version 1903 or later
#endif
#include <sdkddkver.h>
#include <icu.h>
#else
#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <unicode/uchar.h>
#include <unicode/uloc.h>
#include <unicode/uscript.h>
#endif
