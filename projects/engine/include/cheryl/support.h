#pragma once

/** @file
 * @brief Compatibility includes for APIs now owned by selected modules.
 */

#if defined(CHERYL_HAS_SUPPORT_STARTUP) && CHERYL_HAS_SUPPORT_STARTUP
#include <cheryl/support/startup.h>
#endif

#if defined(CHERYL_HAS_SUPPORT_DEBUG_TERMINAL) && CHERYL_HAS_SUPPORT_DEBUG_TERMINAL
#include <cheryl/support/debug-terminal.h>
#endif

#if defined(CHERYL_HAS_SUPPORT_WORKER_TOPOLOGY_HWLOC) && CHERYL_HAS_SUPPORT_WORKER_TOPOLOGY_HWLOC
#include <cheryl/support/worker-topology-hwloc.h>
#endif

#if defined(CHERYL_HAS_SUPPORT_WORKER_TOPOLOGY_JSON) && CHERYL_HAS_SUPPORT_WORKER_TOPOLOGY_JSON
#include <cheryl/support/worker-topology-json.h>
#endif

#if defined(CHERYL_HAS_SUPPORT_OPENGL_STARTUP) && CHERYL_HAS_SUPPORT_OPENGL_STARTUP
#include <cheryl/support/opengl-startup.h>
#endif
