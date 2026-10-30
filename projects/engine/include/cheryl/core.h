#pragma once

/** @file
 * @brief Core system and runtime interfaces without selecting a native backend.
 * Links to the Engine target; concrete platform/graphics/UI implementations remain separate modules.
 * @see [Detailed guide](cheryl-guide:engine-systems/index)
 */
#ifndef CE_CORE_H
#define CE_CORE_H
#include "core/logging.h"
#include "core/resources.h"
#include "core/display.h"
#include "core/controls.h"
#include "core/engine.h"
#include "core/rendering.h"
#include "core/subsystems.h"
#include "core/game-framework.h"
#endif
