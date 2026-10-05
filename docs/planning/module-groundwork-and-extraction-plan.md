# Module groundwork and extraction plan

## Status

G1–G4 are implemented: target ownership/layout, optional Native GLFW and whole OpenGL
owners, standalone composition conventions, module-local tests/consumers and the
coordinated native/OpenGL extraction. The resulting contract is authoritative in
[modules.md](../development/modules.md). G5 establishes executable isolation before
UI, input composition or Steam consumers rely on the new graph.

## G5 — executable isolation and composition acceptance

G5 remains open. Source/static preparation is complete; executable isolation,
composition acceptance and the measured Engine unit build/run cost remain pending.
Maintain its progress checklist in
[module-validation-plan.md](module-validation-plan.md), which owns the ordered
selections, bounded commands and required evidence. Execution requires explicit
build/test authorization.

Implementation-owned checks remain distinct from dummy-backed Engine contract
checks. Record unavailable native/display opt-ins as coverage limits. Native-only
selection does not promise a window-only runtime.

The coverage boundaries in [architecture-validation.md](../development/architecture-validation.md)
remain useful context. Earlier directory-migration runs do not prove this graph;
the reported extracted aggregate run predates the owner-runner composition changes
and does not identify native opt-ins or skips.

Keep the deprecated sandbox shorthand until its consumers migrate to explicit
`CHERYL_NATIVE_NULL_PLATFORM`/`CHERYL_NATIVE_INPUT` choices. Engine-only selection
does not need sandbox mode; mock OpenGL consumers can still require GLFW-null
without Gainput.

## G6 — UI adapter

After the relevant G5 gate, resume U9 with a selected UI consumer. Add only generic
render/resource/input/routing/platform capabilities that the adapter actually needs.
The adapter is an optional owner and does not select OpenGL or own the native event
pump. See [cheryl-ui-integration-plan.md](cheryl-ui-integration-plan.md).

## G7 — optional input composition and Steam policy

When a consumer needs composition, implement these as separate coherent units:

1. Separate source collection from publication. Contributions carry source/device
   identity into one engine coordinator; retain the current default binding scope.
2. Prove composed-input policy with fakes. Merge held contributions before deriving
   button edges, choose absolute-axis conflict policies and consume relative deltas
   once. Publish one poll with one latched focus/capture state and observed record
   order. Each effective controller has one collection owner.
3. Schedule application services independently of input admission and rendered frames.
4. Prove SDK-free Steam session/input policy before adding transport. Session
   ownership is application-scoped; context leases do not shut down another user.

**Acceptance:** Native-only behavior stays intact. Fake native + Steam sources cover
ID collisions, disconnects, focus/capture and immutable prior polls. Holding Jump
from two sources and releasing one keeps it held until the final release. A fake
application service progresses with a full input backlog, without rendered frames
and through startup/shutdown failure. Keep one provider as the default; per-player
APIs wait for a concrete consumer requirement.

## G8 — selected Steam transport and later candidates

Choose Steam SDK/version/application requirements before transport. Keep SDK-native
types and discovery inside the module. Real-client/controller acceptance remains
separate from fake policy checks. Prove controller identification and duplicate-path
suppression before promising per-device Steam/native selection; retain native
keyboard/mouse/text. Additional services, direct event mode and runtime loading
depend on later consumer choices.

Additional extraction requires a concrete omit/replace/test benefit. Another graphics
backend is a valid architectural proof; splitting engine internals simply because a
seam exists is not.

## Development boundary

Module work must preserve owner lifetimes and dependency direction:

No Engine target links back to a module. OpenGL depends on Engine and Native GLFW;
Native GLFW depends on Engine. A future owner may use neutral Engine contracts;
Engine must not acquire that integration as a dependency. Preserve reverse teardown:
input detaches from a live window, GL resources retire or abandon before context
destruction, and runtime worker/frame shutdown retains its existing ordering.

Build/configuration/compiler/test execution follows `AGENTS.md` and requires explicit
authorization. Planning/source inspection does not imply executable acceptance.
