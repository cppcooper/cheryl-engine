# Event capability follow-up

The selected non-font event integration is implemented. This document retains
unselected capability prerequisites; it does not authorize another implementation
batch. Current contracts live in the linked subject guides. The historical
[design assessment](../../../reports/event-coverage/design.md) and
[event catalogue](../../../reports/event-coverage/event-catalogue.csv) retain the
original rationale and stable event IDs, rather than serving as progress logs.

## Current readiness

| Implemented capability | Contract owner |
| --- | --- |
| Typed notifications, weak publisher routes and owned required attachments | [Event delivery](../../site/runtime/event-delivery.md#observe-a-change-or-participate-in-an-operation) |
| Window selection, renderer viewport and automatic UI view/time following | [Committed views](../../site/runtime/display-and-window-contract.md#committed-window-and-simulation-views), [TGUI](../../site/modules/tgui.md#automatic-view-and-time), [RmlUi](../../site/modules/rmlui.md#automatic-view-and-time) |
| Runtime milestones, retained outcomes, negotiable close and owner service phases | [Runtime boundary](../../site/runtime/runtime-frame-boundary.md#lifecycle-observation) |
| Sampled gamepad availability and aggregate upload outcomes | [Input](../../site/runtime/input-state-model.md#gamepad-availability), [upload settlement](../../site/assets/asset-loading.md#upload-settlement-notifications) |
| Exact-key asset catalogues and native-domain provenance/invalidation | [Catalogue changes](../../site/assets/asset-loading.md#catalogue-changes), [graphics domains](../../site/resources/resource-residency.md#graphics-domain-observation) |
| Logical playback outcomes and optional runtime notification pumping | [Audio outcomes](../../site/runtime/audio.md#playback-outcomes), [maintenance attachment](../../site/modules/miniaudio.md#runtime-notification-pump) |

Runtime pressure remains an internal observation. Manual standalone UI and audio
use remains supported. Notification does not replace direct results, game hooks,
joins, queue closure, accepted-work settlement or final native collection.

Focused Linux controlled and native integration coverage exists for these
boundaries; it does not establish physical-controller behavior or hardware audio
outcome delivery. Other platform and SDK-composition coverage remains subject to
its existing [platform plan](../long-term/platform-acceptance.md) and module limits.
Reusable runner selection belongs to the [testing guide](../../site/development/testing.md):
new Engine cases run in `tests-engine`, legacy resource/runtime/bus cases in
`acceptance-engine`, and real GLFW resize/close/render cases in `acceptance-opengl`
(`tests-acceptance-opengl`). Offline logical outcomes use `tests-audio-miniaudio`.
Do not mistake an aggregate, skipped native case or source presence for execution.

## Deferred font catalogue

R3 / RES03 is explicitly deferred. Existing FontService configuration and retained
font identity remain unchanged; current UI sessions import their catalogue once.
If a consumer later requires font catalogue observation, first select its concrete
use and preserve these requirements from the original design:

- A coherent immutable named/default/ordered-fallback snapshot, rather than
  separately locked reads. The embedded initial catalogue has revision zero.
- Successful explicit configuration commits a revision; failed preparation
  preserves the old snapshot. Permitted concurrent configuration callers stage
  observations for one designated publication owner with a race-free watch seed.
- No implicit widget rebinding or native family-alias replacement. RmlUi's
  retained-name restriction and application fallback policy need their own design
  before adopting a new font generation in an existing session.

Do not activate this work merely to match other catalogue event families.

## Deferred capability boundaries

| Candidate | Prerequisite before selecting implementation |
| --- | --- |
| DEV04 DisplayTopologyChanged | Refreshable inventory, connection identity, native-handle invalidation and fallback/rebinding. Follow the existing [monitor plan](../long-term/README.md#monitor-hotplug). |
| DEV05 InputDeviceTopologyChanged | Portable enumeration/connection identities and real backend lifecycle observation; coordinate with [HID](../long-term/README.md#deferred-hid-integration) and native input ownership. Convenience-slot availability is not physical topology. |
| DEV06 AudioOutputStateChanged | Actual output/device/route observation, capability limits and recovery policy. Logical PlaybackEnded does not imply device-loss detection. |
| SYS11 PlatformLifecycleChanged | Real OS suspend/resume/session-ending adapters, owner marshalling and forced/deadline policy. Focus/minimize is insufficient. |
| Independent native pumping | Bounded staging on every pending input path, visible overflow, whole-poll FIFO/capacity and explicit routing/capture latch and spacing semantics across pumps. |

### Independent native pumping

Current observation is sampled and poll-gated: a full input backlog can delay
GLFW close/focus/resize observation. Host shutdown requests and platform
maintenance remain independent of that backlog. Do not add an extra native pump
under saturation without settling the pending-record contract first.

A stronger native-response capability must choose between keeping one logical
poll/capture latch open across several pumps and relatching routing/capture each
pump. Preserve complete immutable poll publication, reconciliation and FIFO
ordering. Bound every pending path and report overflow at a safe owner boundary,
including when no semantic poll completes. Arbitrary lossless input, bounded
memory and endless pumping with a permanently stalled consumer cannot all be
promised. Specify spacing, focus changes, callback failure and standalone
`begin_poll -> pump -> update` behavior before dependent code accumulates.

## Separate consumer decisions

Presentation/density/observed-mode view extensions, live font adoption, native
graphics recovery, complete playback history, residency budgets and automatic
asset hot reload remain separate capabilities. Keep application save/gameplay
policy and absent service domains out of the event layer. The
[planning catalogue](../README.md) and [STATUS](../../STATUS.md) own selection and
review order; choose a concrete consumer and acceptance scope before activating
one of these follow-ups.
