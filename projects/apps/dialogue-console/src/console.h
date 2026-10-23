#pragma once

#include "gate.h"

#include <iosfwd>

namespace GateConsole {
    int run_console(Encounter& encounter, std::istream& input, std::ostream& output);
}
