# Long-term engine and integration plans

This guide retains the scope and prerequisites for deferred integrations and
consumer-driven extensions. RmlUi rendering effects, an alternative audio SDK and
the in-game developer console are now selected in the
[status inventory](../../STATUS.md#2-planned).
Other unselected engine and module extensions still need a named application
consumer to establish scope and prerequisites. Additional asset manifests below
remain deferred to the long term. Steam API services, Steam Input and their
SDK-free preparation are outside the short- and mid-term development sequence.
Multi-platform testing is also shelved; Linux/X11 remains the active short-term
platform.

Use the current ownership, lifetime, selection and standalone contracts in the
[module guide](../../site/development/modules.md). Its
[module boundary criteria](../../site/development/modules.md#module-boundary-criteria)
govern new owners. Reusable isolation/composition procedures and native/fixture
coverage limits are in
[architecture validation](../../site/development/architecture-validation.md).

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
deferred. The [display/window contract](../../site/runtime/display-and-window-contract.md)
allows a display to own several windows but permits one active rendering selection;
replacement of that selection rejects. Window ownership alone does not establish
multiple graphics contexts, presentation surfaces or independent resource domains.

Before reactivating this extension, select a consumer and resolve graphics/context
ownership, presentation, per-window input and size routing, resource sharing and
in-flight frame/teardown lifetimes. Keep broader native input ownership separately
scoped rather than assuming each additional window needs another initialized
Gainput owner.

### Windows worker affinity

Windows worker CPU affinity is deferred to phase 4 of the
[status inventory](../../STATUS.md#3-deferred), alongside platform acceptance and
OS services. The current Windows hwloc provider retains CPU/core/cache discovery
but reports thread binding unavailable because its backend rejects
`HWLOC_CPUBIND_NOMEMBIND`. The [worker contract](../../site/runtime/worker-execution.md#cpu-policy-and-capabilities)
requires CPU binding to preserve memory placement; preferred policies use portable
scheduling and required CPU/cache locality requests reject.

Investigate a Windows calling-thread binding path that preserves memory placement
and inherited CPU eligibility, with native SDK details confined to the provider.
If the OS/backend cannot meet that contract, resolve the contract explicitly before
advertising affinity support. Keep NUMA allocation, first-touch and migration in
their separate indefinite deferral.

Acceptance on native Windows must prove CPU-mask readback, unchanged memory policy
and pool-creating thread affinity, required CPU/cache locality enforcement, and
clear rejection of unsupported requirements. Verify preferred-policy fallback and
provider lifetime through worker shutdown. Successful topology discovery or
portable job execution does not establish binding support.

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
[native capability limits](../../site/modules/native-glfw.md#hid-capability-and-platform-scope)
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
[native adapter lifetime](../../site/modules/native-glfw.md#input-lifetime-and-mapping)
permits one initialized owner and retains its original Windows notification window.
Explicit HID startup-failure reporting follows the
[deferred HID contract](#deferred-hid-integration); ownership checks do not supply
the missing readiness signal.

## Other optional modules

### UI adapters

The RmlUi core integration supplies native documents/fonts, keyboard/text/pointer
input, rectangular clipping, retained CPU/GPU scenes and lifecycle handling. Its
selected Linux root-composition baseline remains usable while rendering effects
are planned. Existing dedicated regressions and supplied-Core acceptance remain in
the [startup/UI follow-up](../short-term/README.md#startup-and-ui-follow-up).

RmlUi masks, nonidentity transforms, offscreen layers, filters, custom shaders and
repeating texture coordinates are planned extensions and currently reject
publication under the
[current recording contract](../../site/modules/rmlui.md#recording-and-publication).
Determine which effects can be implemented by the adapter and which require
changes to neutral render/resource contracts and the graphics backend. Include
necessary OpenGL/backend support in the selected task, covering render targets,
passes, masks, transform/clipping semantics and filter/shader/sampling capabilities
as required by the chosen features. Preserve retained resource generations and
the independently usable core path; prove CPU/native rendering and failure behavior
for each added capability.
Clipboard, cursor styling, IME and controller navigation also need separately
selected consumer requirements and remain deferred. Resolve transform/clipping,
resource lifetime and capability/failure contracts before implementing the selected
effects.

Select another toolkit only for a concrete application need and preserve its native
authoring API. Similarity to existing bridges does not require shared widgets.
Each toolkit remains independently selectable and depends on neutral Engine
contracts; toolkit code must not become an Engine dependency. TGUI and RmlUi
establish the current boundary in the
[adapter-author guide](../../site/development/ui-adapters.md).

### Additional graphics backends

A new backend is useful both as an implementation and as a proof that neutral render/
resource contracts support another API. It owns its SDK/API dependency and
implementation tests. Do not split existing internal engine facilities in
anticipation of an unselected backend. Another extraction requires a concrete
omit/replace/test benefit; a seam alone does not justify it.

## Other engine extensions

### Graphical developer console

The built-in graphical developer console is planned as the engine's own graphical
interface.
Its scope consists of that console, with the renderer supplying its rendering;
other application UI remains consumer-owned. It supplies eventual in-app console
output independently of whether an application selects the optional native terminal.
Establish scrolling/history, input routing and command requirements before
implementation. Keep it independent of the optional
[native terminal](../../site/modules/debug-terminal-linux.md).

### Text coverage

Beyond the initial [grayscale scope](../../site/assets/text-layout.md#native-acceptance-scope),
consider wider CJK/script acceptance when a consumer establishes its required fonts,
language-specific shaping, line-breaking and coverage fixtures. Color emoji is a
planned [nearer text follow-on](../mid-term/README.md#color-emoji).
Neither belongs to the current grayscale Latin/Cyrillic and bidi batch.

### Alternative audio SDK

The [miniaudio module](../../site/modules/miniaudio.md)
implements ordinary playback with an accepted Linux root-assembly WAV baseline.
An alternative audio SDK/module is planned; select its SDK and assess licensing,
runtime/dependency deployment, source/package integration and coexistence before
implementation. FMOD is a candidate for the same ordinary playback contract;
Studio events, banks and adaptive authoring require their own selected scope and
SDK/deployment requirements. Preserve miniaudio's independently usable path and
define consumer/native acceptance for the chosen alternative. Spatial audio,
effects graphs, capture, device enumeration/hotplug and custom codecs also need
separately selected consumer requirements.

### Atlas padding and repacking

Additional atlas padding or repacking needs a separately selected scope. Preserve
the [sampling contract](../../site/rendering/pipelines-and-materials.md#immutable-sampling):
current grid UVs address cell edges without padding. The
[demo samples](../../../projects/apps/demo/README.md#tile-and-sprite-samples)
retain the baseline observations.

### Game systems

Manual collision checks for object overlap/clipping are deferred to phase 1 of the
[status inventory](../../STATUS.md#3-deferred). Define their shape, coordinate and
boundary rules separately from physics simulation. Save-game serialization,
physics, tilemap/world and entity-component systems are deferred to phase 2;
their scope and acceptance still require a game consumer. The game can consume
Cheryl as a Git submodule and supply its own optional modules from the start, or
implement its features directly. Do not plan on a later extraction or game
refactor as a prerequisite for starting development.

### NUMA locality

NUMA discovery and memory placement are deferred indefinitely; no client/server
architecture is scheduled. Reopen only for a consumer and hardware that justify
multi-node locality. Any resumed facility must be optional at compile time and
preserve ordinary submission/completion behavior. Separate CPU-to-node mapping
from allocation, first-touch and migration; single-node runs cannot establish
multi-node acceptance. Preserve the
[worker provider contract](../../site/runtime/worker-execution.md#topology-providers-and-capacity).
Topology alone does not prove a speedup or guarantee residency; performance claims
require representative workload measurements.

### Other extensions

3D, networking, world/entity/physics, serialization,
device-loss recovery and broader OS/device validation remain separate consumer-driven
work. Placeholders do not imply supported facilities. Establish scope, prerequisites,
ownership and acceptance before selecting any candidate in STATUS.

Broader pointer capture, modal/controller routing and clipboard/cursor/IME services
need explicit consumer requirements under the [UI capability boundaries](../../site/development/ui-adapters.md#routing-and-unavailable-services).
Build-tree and [prebuilt SDK consumption](../../site/development/consuming-engine.md#prebuilt-development-sdks)
are implemented. SDK collection and release publication belong to the
[CI workflow](../../../.github/workflows/ci.yml); they are not a deferred engine
extension. The first main-push collection remains in the
[testing queue](../../testing-requests.md#tr20-automated-main-push-sdk-collection).
Hosted build/SDK acceptance does not establish desktop/device acceptance.

### Monitor hotplug

Native monitor hotplug is deferred in
[phase 3 of the status inventory](../../STATUS.md#3-deferred); monitor hotplug and
per-window scale/change reporting still need explicit consumers.
Settle refreshed inventory identity, native-handle invalidation and existing-window
rebinding before adding hotplug support; monitor scale alone does not satisfy a
per-window service. The current
[display/window contract](../../site/runtime/display-and-window-contract.md) defines the
construction-time inventory and copied-size capability limits.

### Residency accounting

Before adding automatic budgets or eviction, assign allocations domain-qualified
identities and account once for staging, live native storage, pending retirement
and externally pinned generations. Distinguish upload-byte estimates from actual
backend allocation, and advisory budgets from enforceable admission limits.
Settle owner-thread admission and independent provider/cache ownership before
allowing multiple domains. Eviction must preserve in-flight frames and avoid
counting shared images/materials repeatedly. The current
[residency contract](../../site/resources/resource-residency.md) supplies strong ownership
and explicit clear/replacement/teardown; cache counts do not provide byte budgets.

## Additional dungeon asset manifests

Prepare one manifest for each of the two 0x72 dungeon packages in the
[download catalog](../../site/assets/catalog.md#packages-awaiting-manifests) when
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
   [manifest format](../../site/assets/asset-manifests.md) and
   [loading validation contract](../../site/assets/asset-loading.md).

**Acceptance:** the catalog identifies each package revision, and its manifest uses
valid image paths and grids; slices and animations match that revision. Schema/source checks and
loader execution establish their respective coverage. Package acquisition
does not establish manifest or runtime acceptance. Community extension/remix packs
remain separate candidates outside these two manifests.
