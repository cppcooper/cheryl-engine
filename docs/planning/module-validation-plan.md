# Module extraction validation

G5 follows the implemented extraction and role-directory organization. The working
tree is clean at the start of this unit. Configuration, compilation and executable
tests require the explicit request specified by AGENTS.md; preparation and static
checks can proceed independently. The owner selected preparation and static checks
only for this pass; no configuration, compilation or executable tests are authorized.

## Sequence

1. Prepare a small Engine-only signal-acceptance consumer and isolated-process driver.
   Its fatal-signal case links only `Cheryl::Engine` and calls no engine function,
   proving automatic bootstrap delivery despite static archive extraction. Compare
   builds with and without `NDEBUG`; exception/explicit trace checks remain separate.
   Do not use the standalone Backward dependency check as evidence for Engine.
   Implementation: [consumer](../../projects/engine/tests/signal-acceptance/src/main.cpp)
   and [manual POSIX driver](../../projects/engine/tests/signal-acceptance/signals.py).
   Source and syntax review completed; build and execution remain pending.
2. Use a fresh Release validation directory with developer logging and one
   low-priority build job. Preserve the existing CLion profiles, whose caches refer
   to another checkout path. Enable the assemblies in order in this one directory
   so completed neutral/dependency objects can be reused:
   - Engine only: neutral first-include probes, consumer, cheap Engine unit runner,
     logging acceptance and the release signal consumer. Capture source/dependency
     inventories and build/run cost without native/graphics SDKs.
   - Engine + Native GLFW: native consumer/probes and real native mapping/diagnostic
     unit cases. Verify that OpenGL/GLAD are not selected.
   - Full OpenGL: OpenGL consumer/probes, module mock suites, selected aggregate,
     native graphics cases and finite demo runs. Record native opt-ins and skips
     separately; keep the native timing/publication case within the focused run.
3. Validate standalone module bootstrap and supplied-target reuse with explicit
   source paths and locally disabled recursive selections. Check module-local test
   registration, one Engine target and owner source/dependency inventories. Build
   the standalone consumers needed to establish the actual link contract.
4. Use a separate Debug Engine-only configuration for the Engine-linked fatal-signal
   check. Run bounded child processes with core files disabled and keep sanitizer
   output distinct from Backward's trace output. Record the actual `NDEBUG` scope.
5. Fix only failures required for this validation unit, committing coherent fixes
   independently. Update extraction evidence and the G5/U9 status from actual
   results; keep any unexecuted or unsupported-host requirement explicit.

## Execution boundaries

Run serially and combine requested targets into as few build invocations as
practical. Use `nice -n 19` and `--parallel 1` for builds. Tests that share fixtures
or log directories run in isolated working directories and serially. Do not repeat
passed checks unless a change, failure or unresolved concern justifies it.

A failure in source ownership, transitive target usage, bootstrap delivery or
standalone composition is part of G5 and must be resolved before dependent adapter
implementation. A host/display limitation is recorded separately from a source
failure. If a difficult design issue would benefit from deeper reasoning, pause
with the evidence and alternatives for owner review as requested.
