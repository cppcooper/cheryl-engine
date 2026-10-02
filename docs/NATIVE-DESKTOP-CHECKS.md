# Desktop smoke checks

## Repeatable sequence

Use the normal build's existing `demo` executable. Run it once with no arguments,
then repeat with `--concurrent`. Keep the run interactive by omitting
`--max-updates`; that option is only for finite automation. The automated normal
configuration uses GLFW's X11 backend with Wayland disabled.

In each mode, check these observations:

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
report is enough to begin investigation. These observations complement the automated
context/lifetime cases in [ARCHITECTURE-VALIDATION.md](ARCHITECTURE-VALIDATION.md).
