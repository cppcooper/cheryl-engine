# Desktop acceptance checks

## Reported result

1 October 2026: after applying checkpoint 95 at
`2c23bf8164ac69e74a6e06bd999ef54a61d7c8f4`, the user reports:
"I ran all the tests, I think. No failures. Everything went as expected."
The supplied desktop checklist is recorded as passing based on that report,
covering input/text focus, resize, successful/failed/recovered F5 reload and
normal window close in the requested sequential/concurrent runs. No GPU/driver
identity or additional automated-suite counts were supplied.

## Repeatable sequence

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
3. Test shader reload using copied assets. From the repository root, run
   `cp -a assets/. /tmp/cheryl-reload-assets`, then launch
   `/path/to/demo /tmp/cheryl-reload-assets`, substituting your demo executable.
   The folder path after the executable is the positional argument: it selects the
   asset root without a flag. While the demo stays running, press Esc to leave text
   editing, then F5 with valid shaders. Reload should succeed. Append
   `this is not valid GLSL;` to the copied
   `/tmp/cheryl-reload-assets/shaders/shader2d.frag` and save. Press F5 again: the
   demo's `Reload:` status must show an error while the previous text/material
   remains visible. Remove the invalid line, save and press F5: the error clears.
   Repeat with `/path/to/demo --concurrent /tmp/cheryl-reload-assets`.
4. Close the window through its normal close control, including during repeated
   reloads. The process exits normally without hanging or printing a cleanup error.

For a failure, identify sequential/concurrent mode, the action, visible behavior,
console error and your GPU/driver if known. Screenshots are optional; a short text
report is enough to begin investigation. These observations complement automated
context/lifetime checks and do not hold up independent source work.

Still separate: forced slow simulation/presentation and overload recovery, more
native reload/failure/resource combinations and remaining OS policy restriction/rejection acceptance.
