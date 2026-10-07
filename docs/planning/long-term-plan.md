# Long-term engine and integration plans

This work is deferred until a named application consumer establishes its scope and
prerequisites. Steam API services, Steam Input and their SDK-free preparation are
outside the short- and mid-term development sequence.

Use the current ownership, lifetime, selection and standalone contracts in the
[module guide](../development/modules.md). Its
[module boundary criteria](../development/modules.md#module-boundary-criteria)
govern new owners. Reusable isolation/composition procedures and native/fixture
coverage limits are in
[architecture validation](../development/architecture-validation.md).

## Steam API and input integration

Steam services and Steam Input may initially share one application-facing module.
Select the required services and controller behavior before implementation; keep one
input provider as the default. Input composition requires a demonstrated consumer
case, such as native keyboard/mouse with Steam-managed controllers. Per-player APIs
wait for a concrete consumer requirement.

### Policy before SDK transport

Implement these as separate coherent units when the consumer requires them:

1. Separate source collection from publication. Contributions carry source/device
   identity into one engine-owned coordinator and one published input snapshot;
   retain the current default binding scope. Compose contributions rather than
   concatenating complete provider snapshots. SDK collection belongs to its module.
2. Prove composed-input policy with SDK-free fakes. Merge held contributions before
   deriving button edges, choose absolute-axis conflict policies and consume relative
   deltas once. Publish one poll with one latched focus/capture state and observed
   record order. Each effective controller has one collection owner.
3. Schedule application services independently of input admission and rendered frames.
   Steam callbacks must progress when ordinary input publication pauses or
   backpressures.
4. Prove SDK-free Steam session/input policy before adding transport. Session
   ownership is application-scoped; context leases do not shut down another user.

**Acceptance:** Native-only behavior stays intact. Fake native + Steam sources cover
ID collisions, disconnects, focus/capture and immutable prior polls. Holding Jump
from two sources and releasing one keeps it held until the final release. A fake
application service progresses with a full input backlog, without rendered frames
and through startup/shutdown failure.

### Selected Steam transport

Choose Steam SDK/version/application requirements before transport. Keep SDK-native
types and discovery inside the module. Real-client/controller acceptance remains
separate from fake policy checks. Prove controller identification and duplicate-path
suppression before promising per-device Steam/native selection; retain native
keyboard/mouse/text. Additional services, direct event mode and runtime loading
depend on later consumer choices.

### Broader native input ownership

Multiple initialized native adapters and Windows notification-window rebinding need
a concrete consumer requirement. Coordinate process-wide Gainput HID collection,
shutdown and notification ownership before supporting them; independent `Init`/`Exit`
calls cannot establish that behavior. Preserve one collection path per effective
controller. The current
[native adapter lifetime](../../projects/modules/platform/native-glfw/README.md#input-lifetime-and-mapping)
permits one initialized owner and retains its original Windows notification window.

## Other optional modules

### UI adapters

Select another toolkit only for a concrete application need and preserve its native
authoring API. Similarity to existing bridges does not require shared widgets.
Each toolkit remains independently selectable and depends on neutral Engine
contracts; toolkit code must not become an Engine dependency. TGUI and RmlUi
establish the current boundary in the
[adapter-author guide](../development/ui-adapters.md).

### Additional graphics backends

A new backend is useful both as an implementation and as a proof that neutral render/
resource contracts support another API. It owns its SDK/API dependency and
implementation tests. Do not split existing internal engine facilities in
anticipation of an unselected backend. Another extraction requires a concrete
omit/replace/test benefit; a seam alone does not justify it.

## Other engine extensions

3D, topology/NUMA adapters, audio, networking, world/entity/physics, serialization,
device-loss recovery and broader OS/device validation remain separate consumer-driven
work. Placeholders do not imply supported facilities. Establish scope, prerequisites,
ownership and acceptance before promoting any candidate into the development roadmap.

## Development boundary

Module work preserves owner lifetimes and dependency direction. A future owner uses
neutral Engine contracts; Engine must not acquire that integration as a dependency.

Build/configuration/compiler/test execution follows [AGENTS.md](../../AGENTS.md)
and requires explicit authorization. Planning/source inspection does not imply
executable acceptance.
