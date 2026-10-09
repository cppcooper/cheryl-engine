# Planning catalogue

Horizons describe sequencing, not delivery dates. Detailed checklists live in the
plan owning each active task; current contracts live in the [subject guides](../README.md).
The [status inventory](../STATUS.md) records implementation classifications.
The [roadmap](develop-review-and-development-plan.md#nearest-planned-work) owns the
ordered list of nearest planned work,
and [testing requests](../testing-requests.md) contains pending user-run acceptance.

| Folder | Scope and owning plans |
| --- | --- |
| `short-term/` | [Shader/material manifests and indexed loading](short-term/indexed-material-loading.md), [external Debug terminal](short-term/debug-console.md) and [Unicode font-selection regressions](short-term/unicode-text.md). The roadmap owns texture sampling/TGUI, NUMA/NUCA, render batching and [startup/UI follow-up](develop-review-and-development-plan.md#startup-and-ui-follow-up). |
| `mid-term/` | [Color emoji, reorder-safe sorting and measured text/timing optimization](mid-term/README.md), gated by selected scope and workload evidence |
| `long-term/` | [Deferred integrations and consumer-selected extensions](long-term/README.md), [deferred HID/platform acceptance](long-term/platform-acceptance.md) and [unresolved artwork metadata](long-term/asset-manifest-metadata.md) |

The [native integration deferrals](long-term/README.md#deferred-native-integration)
retain multiple active rendering windows, HID backend work and their prerequisites.
The current RmlUi core integration is sufficient for the selected Linux scope;
[further UI extensions](long-term/README.md#ui-adapters) remain deferred.
