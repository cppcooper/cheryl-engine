#pragma once

/** @file
 * @brief Workers module entry; link Cheryl::Workers. Includes JSON/hwloc declarations only when their public feature definitions are present.
 * @see [Detailed guide](cheryl-guide:modules/workers)
 */

#if defined(CHERYL_HAS_SUPPORT_WORKER_TOPOLOGY_JSON)
#include <cheryl/core/engine/worker-topology/configuration.h>
#endif
#if defined(CHERYL_HAS_SUPPORT_WORKER_TOPOLOGY_HWLOC)
#include <cheryl/core/engine/worker-topology/hwloc.h>
#endif
