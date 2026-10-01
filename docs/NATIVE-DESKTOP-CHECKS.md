# Desktop acceptance checks

Use the normal build's existing `demo` executable. Run it once with no arguments,
then repeat with `--concurrent`. Keep the run interactive by omitting
`--max-updates`; that option is only for finite automation. The automated normal
configuration uses GLFW's X11 backend with Wayland disabled.

For each mode, report pass/fail and any console error for these observations:

1. Text renders clearly; WASD pans the camera and R resets it. Mouse coordinates,
   click count and wheel values respond. Resize the window several times: text
   remains correctly placed and the controls remain responsive.
2. Press F2, type ASCII text, move the caret and use Backspace/Delete. Text focus
   prevents those keystrokes from moving the camera. Enter/Esc releases focus.
   The existing ASCII atlas displays fallback characters for unsupported Unicode;
   this is not a Unicode shaping check.
3. Press F5 with valid shaders. Rendering continues without a reload error. For
   failure/recovery, launch with a temporary asset-root copy as the positional
   argument, introduce a syntax error in its `shaders/shader2d.frag`, and press F5.
   A reload error must appear while the previous text/material continues rendering.
   Restore that copied shader and press F5: the error clears and rendering continues.
   Keep the repository's original assets intact.
4. Close the window through its normal close control, including during repeated
   reloads. The process exits normally without hanging or printing a cleanup error.

For a failure, identify sequential/concurrent mode, the action, visible behavior,
console error and your GPU/driver if known. Screenshots are optional; a short text
report is enough to begin investigation. These observations complement automated
context/lifetime checks and do not hold up independent source work.

Still separate: forced slow simulation/presentation and overload recovery, more
native reload/failure/resource combinations, rotated FFont output, stb internal
allocation faults and remaining OS policy restriction/rejection acceptance.
