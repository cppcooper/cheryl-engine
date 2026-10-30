# Planning catalogue

Horizons describe sequencing, not delivery dates. Detailed checklists live in the
plan owning each active task; current contracts live in the [subject guides](../README.md).
The [status inventory](../STATUS.md) owns implementation classifications, selected
work and deferred review order. [Testing requests](../testing-requests.md) owns
ready pending user-run acceptance; the plans below retain unfinished work and
its prerequisites.

## Find the owning plan

Use this table to reach the actual requirements. Selection and deferred phase order
remain in STATUS; a horizon is not a delivery commitment.

| Horizon | Plan or section | Next checkpoint |
| --- | --- | --- |
| Deferred follow-up | [Event capabilities](short-term/engine-events.md) | Selected non-font integration is implemented. Select a consumer before font catalogue, independent native pumping or further device/OS event work. |
| Short term | [Font-selection regressions](short-term/README.md#font-selection-regressions) | Cover automatic weight boundaries and explicit preferences. |
| Short term | [Startup and UI follow-up](short-term/README.md#startup-and-ui-follow-up) | Cover successful context/runtime handoff, callback/batch and placeholder cases; obtain a corrected supplied RmlUi host. |
| Short term | [Upstream placeholder follow-up](short-term/README.md#upstream-placeholder-follow-up) | Reproduce against unmodified upstream code before proposing a regression or submission. |
| Selected review | [Dependency reviews](#dependency-reviews) | Assess GoogleTest, nlohmann/json and spdlog independently; recommend exact pins. |
| Mid term | [Color emoji](mid-term/README.md#color-emoji) | Select formats and sequences; settle RGBA pages, materials and fallback. |
| Mid term | [Reorder-safe sorting](mid-term/README.md#reorder-safe-render-sorting) | Identify reorderable regions and a representative workload. |
| Mid term | [General batching](mid-term/README.md#general-render-batching) and [measured optimization](mid-term/README.md#measured-optimization) | Gather measurements before selecting an implementation. |
| Long term | [UI extensions](long-term/README.md#ui-adapters) | Settle RmlUi effects and required rendering/backend contracts. |
| Long term | [Alternative audio](long-term/README.md#alternative-audio-sdk) and [graphical console](long-term/README.md#graphical-developer-console) | Settle SDK/deployment or command/history/input requirements. |
| Long term | [Native integration](long-term/README.md#deferred-native-integration) and [platform acceptance](long-term/platform-acceptance.md) | Select a consumer/platform and supply the backend or observation prerequisites. |
| Long term | [Steam services and input](long-term/README.md#steam-api-and-input-integration) | Establish SDK-free input/session policy before selecting transport. |
| Long term | [Residency/cache domains](long-term/README.md#residency-accounting) and [monitor hotplug](long-term/README.md#monitor-hotplug) | Settle accounting or inventory identity/lifetime before adding policy. |
| Long term | [Atlas padding](long-term/README.md#atlas-padding-and-repacking) and [NUMA locality](long-term/README.md#numa-locality) | Select scope for padding; NUMA remains indefinitely deferred. |
| Long term | [Artwork metadata](long-term/asset-manifest-metadata.md) | Resolve source metadata and consumer-specific manifest choices. |
| Long term | [Additional dungeon manifests](long-term/README.md#additional-dungeon-asset-manifests) | Inventory acquired packages and author each manifest independently. |

For implemented work awaiting execution, use [STATUS's acceptance summary](../STATUS.md#remaining-acceptance)
and the [testing queue](../testing-requests.md). Those procedures are not development
plans.

Deferred phases in the status inventory describe review order within that section;
they do not replace these subject horizons or authorize implementation.

The [native integration deferrals](long-term/README.md#deferred-native-integration)
retain multiple active rendering windows, HID backend work and their prerequisites.
The current RmlUi core integration is sufficient for the selected Linux scope;
[rendering effects](long-term/README.md#ui-adapters) are planned, including any
required backend support. OS services and additional toolkit selection remain
deferred.

## Dependency reviews

The three reviews selected in [STATUS](../STATUS.md#2-planned) concern GoogleTest,
nlohmann/json and spdlog. Keep them independent.

For each dependency, compare its pinned commit with upstream history and candidate
revisions, including changed ancestry and tag/commit availability. Review release
notes, integration/API changes, CMake targets/options, supported compilers and
license changes. Check Cheryl-owned source and supplied target/package paths where
applicable. Recommend an exact reproducible candidate or retention of the current
pin; a review does not itself select an update.

If an update is selected, keep each submodule change a separate development unit.
Design focused acceptance for affected owners, independent consumers and the
Linux/Windows CI configurations. Preserve unrelated pins and local dependency work.

## Topics awaiting plans

The [deferred STATUS entries](../STATUS.md#3-deferred) include topics with no
concrete plan yet: expanded manifest formats and automatic asset hot reload.
ECS, tilemap/world, physics, save serialization, 3D and networking currently have
only a [consumer-required deferral](long-term/README.md#game-systems). Spatial
audio and OS/UI services likewise need specific design and acceptance checkpoints
beyond their [audio](long-term/README.md#alternative-audio-sdk) and
[UI](long-term/README.md#ui-adapters) scope notes.

When one is selected, establish its consumer, scope, contracts, prerequisites and
observable acceptance in an owning plan, then link that plan from this index and
STATUS. Conditional possibilities mentioned in a plan remain unselected until
promoted explicitly. When an actual plan completes, preserve its resulting
contracts and unresolved acceptance, remove the completed plan and reconcile both
indexes.
