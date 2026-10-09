# Long-term engine and integration plans

Selected integration deferrals and consumer-driven extensions live here. Unselected
engine and module extensions need a named application consumer to establish scope
and prerequisites. Additional asset manifests below are also deferred to the long
term. Steam API services, Steam Input and their SDK-free preparation are outside
the short- and mid-term development sequence.
Multi-platform testing is also shelved; Linux/X11 remains the active short-term
platform.

Use the current ownership, lifetime, selection and standalone contracts in the
[module guide](../../development/modules.md). Its
[module boundary criteria](../../development/modules.md#module-boundary-criteria)
govern new owners. Reusable isolation/composition procedures and native/fixture
coverage limits are in
[architecture validation](../../development/architecture-validation.md).

## Multi-platform acceptance

The [deferred platform plan](platform-acceptance.md) owns Windows native input,
typed-event/resize, desktop and controller acceptance, Windows HID notification
observations, and macOS/Wayland/other-platform prerequisites. It preserves the
Windows TR3–TR5 procedures and Linux/Windows TR6 outside the active
testing queue. Resume each platform only when testing is scheduled; reconcile its
dependency/toolchain configuration and observation harness before reactivating
requests. Linux source development and Linux/X11 acceptance proceed independently.

Mobile and console work requires a selected platform consumer before scheduling.
For mobile, establish the backend/device target, touch and window lifecycle,
suspend/resume, graphics context, asset delivery and packaging, then require an
end-to-end demo and reproducible consumer build before store deployment. A console
owner must isolate proprietary SDK/toolchain code behind neutral contracts. These
are selection prerequisites; no mobile or console module is implemented.

## Deferred native integration

### Multiple active rendering windows

Multiple active rendering windows and switching the selected rendering window are
deferred. The [display/window contract](../../runtime/display-and-window-contract.md)
allows a display to own several windows but permits one active rendering selection;
replacement of that selection rejects. Window ownership alone does not establish
multiple graphics contexts, presentation surfaces or independent resource domains.

Before reactivating this extension, select a consumer and resolve graphics/context
ownership, presentation, per-window input and size routing, resource sharing and
in-flight frame/teardown lifetimes. Keep broader native input ownership separately
scoped rather than assuming each additional window needs another initialized
Gainput owner.

### Deferred HID integration

Gainput HID backend work remains deferred at the `d94c60f` code baseline. The
[submodule handoff](../../../extern/gainput/TODO.md#ordered-implementation) owns its
input/adaptive-trigger sequence. Enabling HID requires compiler-definition,
report identity/retained-state and source-selection corrections together, preserving
one initialized native owner and one report source per controller.

The Gainput-driven non-HID controller path remains usable. Linux joystick
acceptance uses HID disabled and does not establish USB/HID report delivery.
[TR6](platform-acceptance.md#deferred-hid-lifecycle-and-windows-notifications)
preserves Linux USB/Bluetooth lifecycle and Windows notification requirements
outside the active testing queue. Backend work and an observation harness must be
ready before reactivating it. Successful Gainput `Init` and the demo trace do not
establish HID readiness: the dependency discards HID initialization's return code,
so explicit startup-failure reporting requires a contract change if selected.

Broader native ownership requires a consumer case. Battery and touch/motion
exposure remain separate from the handoff's initial scope; use the
[native capability limits](../../../projects/modules/platform/native-glfw/README.md#hid-capability-and-platform-scope)
when selecting extensions. Windows/macOS source, fallback and notification work
remains with the deferred platform scope.

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
[native adapter lifetime](../../../projects/modules/platform/native-glfw/README.md#input-lifetime-and-mapping)
permits one initialized owner and retains its original Windows notification window.
Explicit HID startup-failure reporting follows the
[deferred HID contract](#deferred-hid-integration); ownership checks do not supply
the missing readiness signal.

## Other optional modules

### UI adapters

The RmlUi core integration supplies native documents/fonts, keyboard/text/pointer
input, rectangular clipping, retained CPU/GPU scenes and lifecycle handling. Its
selected Linux root-composition baseline is sufficient to defer further integration
extensions. Existing dedicated regressions and supplied-Core acceptance remain in
the [startup/UI follow-up](../develop-review-and-development-plan.md#startup-and-ui-follow-up).

RmlUi masks, nonidentity transforms, offscreen layers, filters, custom shaders and
repeating texture coordinates remain deferred and reject publication under the
[current recording contract](../../../projects/modules/ui/rmlui/README.md#recording-and-publication).
Clipboard, cursor styling, IME and controller navigation also need separately
selected consumer requirements. Resolve transform/clipping, resource lifetime and
capability/failure contracts before promoting advanced effects.

Select another toolkit only for a concrete application need and preserve its native
authoring API. Similarity to existing bridges does not require shared widgets.
Each toolkit remains independently selectable and depends on neutral Engine
contracts; toolkit code must not become an Engine dependency. TGUI and RmlUi
establish the current boundary in the
[adapter-author guide](../../development/ui-adapters.md).

### Additional graphics backends

A new backend is useful both as an implementation and as a proof that neutral render/
resource contracts support another API. It owns its SDK/API dependency and
implementation tests. Do not split existing internal engine facilities in
anticipation of an unselected backend. Another extraction requires a concrete
omit/replace/test benefit; a seam alone does not justify it.

## Other engine extensions

Add a built-in graphical developer console as the engine's own graphical interface.
Its scope consists of that console, with the renderer supplying its rendering;
other application UI remains consumer-owned. It supplies eventual in-app console
output, including in Release, where the native Debug terminal does not exist.
Establish scrolling/history, input routing and command requirements before
implementation. Until then, Release uses its existing logging destinations. The
near-term [Debug output console](../short-term/debug-console.md) uses a native terminal and can
provide diagnostics independently of game rendering; closing it leaves the
application running.

Beyond the initial [grayscale scope](../../assets/text-layout.md#native-acceptance-scope),
consider wider CJK/script acceptance when a consumer establishes its required fonts,
language-specific shaping, line-breaking and coverage fixtures. Color emoji is a
possible [nearer text follow-on](../mid-term/README.md#color-emoji).
Neither belongs to the current grayscale Latin/Cyrillic and bidi batch.

The [miniaudio module](../../../projects/modules/audio/miniaudio/README.md)
implements ordinary playback with an accepted Linux root-assembly WAV baseline.
A later FMOD backend may implement the same
ordinary playback contract; Studio events, banks and adaptive authoring require
their own scope and SDK/deployment requirements. Spatial audio, effects graphs,
capture, device enumeration/hotplug and custom codecs also need separately selected
consumer requirements.

Selectable image filtering and TGUI sampling are now
[near-term work](../develop-review-and-development-plan.md#texture-sampling-and-tgui).
That design owns nearest/linear, mipmap/wrap, image/binding ownership and required
atlas-edge behavior. Additional atlas padding or repacking remains separate unless
needed for the selected sampling contract. Current grid UVs address cell edges
without padding; the [demo samples](../../../projects/apps/demo/README.md#tile-and-sprite-samples)
retain the baseline observations.

World, entities, collision and game mechanics wait for a game consumer. The game
can consume Cheryl as a Git submodule and supply its own optional modules from
the start, or implement its features directly. Do not plan on a later extraction
or game refactor as a prerequisite for starting development.

NUMA/NUCA integration is selected in the
[near-term roadmap](../develop-review-and-development-plan.md#numa-and-nuca), with
topology, CPU/memory placement and hardware-capability decisions still open.

3D, networking, world/entity/physics, serialization,
device-loss recovery and broader OS/device validation remain separate consumer-driven
work. Placeholders do not imply supported facilities. Establish scope, prerequisites,
ownership and acceptance before promoting any candidate into the development roadmap.

Broader pointer capture, modal/controller routing and clipboard/cursor/IME services
need explicit consumer requirements under the [UI capability boundaries](../../development/ui-adapters.md#routing-and-unavailable-services).
Installed/exported packaging remains separate work from supported build-tree composition.

Native monitor hotplug and per-window scale/change reporting need explicit consumers.
Settle refreshed inventory identity, native-handle invalidation and existing-window
rebinding before adding hotplug support; monitor scale alone does not satisfy a
per-window service. The current
[display/window contract](../../runtime/display-and-window-contract.md) defines the
construction-time inventory and copied-size capability limits.

### Residency accounting

Before adding automatic budgets or eviction, assign allocations domain-qualified
identities and account once for staging, live native storage, pending retirement
and externally pinned generations. Distinguish upload-byte estimates from actual
backend allocation, and advisory budgets from enforceable admission limits.
Settle owner-thread admission and independent provider/cache ownership before
allowing multiple domains. Eviction must preserve in-flight frames and avoid
counting shared images/materials repeatedly. The current
[residency contract](../../resources/resource-residency.md) supplies strong ownership
and explicit clear/replacement/teardown; cache counts do not provide byte budgets.

## Additional dungeon asset manifests

Prepare one manifest for each of the two 0x72 dungeon packages in the
[download catalog](../../assets/catalog.md#packages-awaiting-manifests) when
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
   [manifest format](../../assets/asset-manifests.md) and
   [loading validation contract](../../assets/asset-loading.md).

**Acceptance:** the catalog identifies each package revision, and its manifest uses
valid image paths and grids; slices and animations match that revision. Schema/source checks and
loader execution establish their respective coverage. Package acquisition
does not establish manifest or runtime acceptance. Community extension/remix packs
remain separate candidates outside these two manifests.
