# Long-term engine and integration plans

Engine and module extensions are deferred until a named application consumer
establishes their scope and prerequisites. Additional asset manifests below are also
deferred to the long term. Steam API services, Steam Input and their SDK-free
preparation are outside the short- and mid-term development sequence.
Multi-platform testing is also shelved; Linux/X11 remains the active short-term
platform.

Use the current ownership, lifetime, selection and standalone contracts in the
[module guide](../development/modules.md). Its
[module boundary criteria](../development/modules.md#module-boundary-criteria)
govern new owners. Reusable isolation/composition procedures and native/fixture
coverage limits are in
[architecture validation](../development/architecture-validation.md).

## Multi-platform acceptance

The [deferred platform plan](platform-acceptance.md) owns Windows native input,
typed-event/resize, desktop and controller acceptance, Windows HID notification
observations, and macOS/Wayland/other-platform prerequisites. It preserves the
Windows TR3–TR5 procedures and deferred Windows portion of TR6 outside the active
testing queue. Resume each platform only when testing is scheduled; reconcile its
dependency/toolchain configuration and observation harness before reactivating
requests. Linux source development and Linux/X11 acceptance proceed independently.

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
If a consumer requires explicit HID startup-failure reporting, change the dependency
contract before relying on it: the selected Gainput `Init` currently discards the HID
initialization return code. Ownership checks do not supply that missing readiness signal.

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

After the initial [Unicode layout task](develop-review-and-development-plan.md#u11--unicode-text-layout-and-glyph-resources),
consider wider CJK/script acceptance when a consumer establishes its required fonts,
language-specific shaping, line-breaking and coverage fixtures. Color emoji is a
possible [nearer text follow-on](develop-review-and-development-plan.md#u11-follow-on--color-emoji).
Neither belongs to the current grayscale Latin/Cyrillic and bidi batch.

3D, topology/NUMA adapters, audio, networking, world/entity/physics, serialization,
device-loss recovery and broader OS/device validation remain separate consumer-driven
work. Placeholders do not imply supported facilities. Establish scope, prerequisites,
ownership and acceptance before promoting any candidate into the development roadmap.

Native monitor hotplug and per-window scale/change reporting need explicit consumers.
Settle refreshed inventory identity, native-handle invalidation and existing-window
rebinding before adding hotplug support; monitor scale alone does not satisfy a
per-window service. The current
[display/window contract](../runtime/display-and-window-contract.md) defines the
construction-time inventory and copied-size capability limits.

## Additional dungeon asset manifests

Prepare one manifest for each of the two 0x72 dungeon packages in the
[download catalog](../assets/catalog.md#downloaded-packages-awaiting-manifests) when
this long-term task becomes active. Keep the original downloads available for source
comparison. Preserve the existing Buch dungeon image and manifest; the added packages
need distinct namespaces and image directories.

Handle one package per coherent development unit:

1. Unpack the saved archive into its own image tree and inventory renderable sheets,
   helper images, editor data and author documentation for that revision.
2. Establish cell bounds, pivots, named slices, action/facing rows and animation
   timing from the matching artwork and supplied metadata. Record unresolved
   semantics rather than inferring them from dimensions alone.
3. Write a separate JSON manifest covering the selected package images and add its
   final locations and manifest link to the catalog. Use the current
   [manifest format](../assets/asset-manifests.md) and
   [loading validation contract](../assets/asset-loading.md).

**Acceptance:** the catalog identifies each package revision, and its manifest uses
valid image paths and grids; slices and animations match that revision. Schema/source checks and
authorized loader execution establish their respective coverage. Package acquisition
does not establish manifest or runtime acceptance. Community extension/remix packs
remain separate candidates outside these two manifests.

## Development boundary

Module work preserves owner lifetimes and dependency direction. A future owner uses
neutral Engine contracts; Engine must not acquire that integration as a dependency.

Build/configuration/compiler/test execution follows [AGENTS.md](../../AGENTS.md)
and requires explicit authorization. Planning/source inspection does not imply
executable acceptance.
