#pragma once

#include <core/controls/input-interface.h>
#include <core/display/display-system-interface.h>
#include <core/display/window-interface.h>

// Scaffold only: the adapters will implement the existing Engine contracts.
// Native SDL3 types, library lifetime and event collection remain private here.
namespace CE::Platform::SDL3 {
    class Window;
    class DisplaySystem;
    class InputSystem;
}
