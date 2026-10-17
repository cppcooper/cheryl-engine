#pragma once

/** @file
 * @brief All backend-neutral public Engine headers.
 */

#include <cheryl/assets.h>
#include <cheryl/audio.h>
#include <cheryl/cemath.h>
#include <cheryl/core.h>
#include <cheryl/enums.h>
#include <cheryl/templates.h>

#include <cheryl/assets/geometry/grid-geometry.h>
#include <cheryl/assets/resources/shader-asset-builder.h>
#include <cheryl/assets/types/3d.h>
#include <cheryl/core/engine/event-delivery.h>
#include <cheryl/core/engine/worker-configuration.h>
#include <cheryl/core/resources/asset-management/shader-asset-mgr.h>
#include <cheryl/core/resources/memory/managed-block.hpp>
#include <cheryl/math/string-numbers.h>
#include <cheryl/templates/observed-variables.h>
#include <cheryl/text/font-collection.h>
#include <cheryl/text/font-rasterizer.h>
#include <cheryl/text/layout.h>
#include <cheryl/text/utf8.h>
