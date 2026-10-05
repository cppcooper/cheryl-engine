# Subsystem module planning

## Current boundary

The initial module extraction is implemented. The authoritative current composition,
target names, dependency direction, standalone convention and test ownership are in
[modules.md](../development/modules.md). Executable extraction acceptance is still G5
in [module-validation-plan.md](module-validation-plan.md). This file defines selection
criteria and future candidates; their sequence is in
[module-groundwork-and-extraction-plan.md](module-groundwork-and-extraction-plan.md).

Cheryl keeps one shared `Cheryl::Engine`. A separate module is justified when it gives
an application a concrete choice or isolation benefit: the ability to omit a dependency,
select an alternative implementation, or test an external integration independently.
A directory, internal interface or small caller count does not by itself justify a
library boundary.

The current selected optional owners are Native GLFW and the whole OpenGL backend.
Keep each cohesive owner intact unless a later consumer demonstrates a real benefit
from another boundary. Native input depends on a live window; dividing display and
input would split that coupled lifetime. OpenGL's context interface permits another
window integration within its owner and does not itself justify context/bridge
libraries.

## Future candidates

### UI adapters

Each toolkit is independently selectable and depends on neutral engine contracts.
Toolkit code must not become an engine dependency. The first concrete work is U9;
see [cheryl-ui-integration-plan.md](cheryl-ui-integration-plan.md).

### Steam integration

Steam services and Steam Input may initially share one application-facing module.
Before SDK transport, prove application/session lifetime, callback progress and any
input-composition policy with SDK-free fakes. Steam callbacks must continue to
progress when ordinary input publication pauses or backpressures.

SDK/version/application requirements are selected before transport work. SDK-native
types and discovery remain inside the owning module.

### Input providers and composition

One provider remains the default. Add composition only for a demonstrated case such
as native keyboard/mouse combined with Steam-managed controllers.

Selected sources feed one engine-owned coordinator and one published input snapshot.
Each effective controller has one collection path. Compose source contributions
before publication rather than concatenating complete provider snapshots; G7 defines
the merge and fake-acceptance gates. SDK collection belongs to its owning module.

### Additional graphics backends

A new backend is useful both as an implementation and as a proof that neutral render/
resource contracts support another API. It owns its SDK/API dependency
and implementation tests. Do not split existing internal engine facilities in
anticipation of an unselected backend.

## Extraction rule

Before introducing another library target, record:

1. the concrete omit/replace/test benefit;
2. the engine contract the owner implements;
3. dependencies that become optional or isolated;
4. lifetime/dependency direction and how cycles are avoided;
5. test ownership and the smallest independent consumer proving the boundary;
6. compatibility consequences for current consumers.

If those points do not identify a real benefit, keep the code inside the existing
owner. Logging, memory, workers, events, general utilities and similar internal
facilities remain engine organization unless a future consumer demonstrates otherwise.

Future extraction must preserve architectural interfaces because of their contract,
not delete them merely because they currently have few callers.
