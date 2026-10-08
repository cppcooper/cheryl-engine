# Planning catalogue

Horizons describe sequencing, not delivery dates. Detailed checklists live in the
plan owning each active task; current contracts live in the [subject guides](../README.md).
The [roadmap](develop-review-and-development-plan.md) records the dependency order,
and [testing requests](../testing-requests.md) contains pending user-run acceptance.

| Folder | Scope and owning plans |
| --- | --- |
| `short-term/` | [Unicode text](short-term/unicode-text.md), [audio](short-term/audio-integration.md) and [demo assets](short-term/demo-assets.md): source complete, Linux native observations pending |
| `mid-term/` | [Color emoji and measured optimization](mid-term/README.md), gated by initial acceptance and workload evidence |
| `long-term/` | [Consumer-selected extensions](long-term/README.md), [deferred platform acceptance](long-term/platform-acceptance.md) and [unresolved artwork metadata](long-term/asset-manifest-metadata.md) |
| `unscheduled/` | [Debug output console](unscheduled/debug-console.md): requirements selected, implementation schedule unset |

Gainput backend/HID work remains deferred in the [submodule handoff](../../extern/gainput/TODO.md)
and [HID roadmap section](develop-review-and-development-plan.md#deferred-hid-integration).
Linux/X11 work proceeds independently of deferred Windows, macOS and Wayland
coverage. Steam API/Input and SDK-free preparation are long-term consumer-driven work.
The native Debug terminal and later built-in graphical console have distinct scopes.
