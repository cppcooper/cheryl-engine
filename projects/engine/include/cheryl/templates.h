#pragma once

/** @file
 * @brief Engine cache, timer, singleton and state utility templates.
 * Each type keeps its own synchronization/lifetime contract; inclusion creates no process service.
 * @see [Detailed guide](cheryl-guide:runtime/utility-contracts)
 */
#include "templates/asset-mgr.h"
#include "templates/delta.h"
#include "templates/singleton.h"
#include "templates/state-machine.h"
#include "templates/state-tracker.h"
#include "templates/versioned-variable.h"
