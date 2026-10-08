# Development roadmap

The [planning catalogue](README.md) groups work by horizon. Linux/GLFW/X11/OpenGL
is the active native platform; deferred platform coverage does not gate Linux work.
Current architecture and API contracts live in the [subject guides](../README.md).

## Short term

Complete the remaining observations for these implemented facilities. Each owning
plan retains its checklist until acceptance is complete:

| Task | Owning plan | Remaining acceptance |
| --- | --- | --- |
| Unicode layout and glyph resources | [Unicode text](short-term/unicode-text.md) | Native appearance, font selection, wrapping and replacement in both runtime modes |
| Audio playback | [Audio integration](short-term/audio-integration.md) | Audible native output and sustained WAV streaming |
| Demo tile/sprite showcase | [Demo assets](short-term/demo-assets.md) | Artwork, playback, optional-load failures and shader replacement |

The [testing queue](../testing-requests.md) owns launch commands and prerequisites.
CPU checks cannot replace these native observations. Reuse the matching accepted
builds when their source and configuration still agree.

### Deferred HID integration

Gainput backend work remains deferred at the `d94c60f` code baseline. The
[submodule handoff](../../extern/gainput/TODO.md#ordered-implementation) owns its
sequence. Enabling HID requires compiler-definition, report identity/retained-state
and source-selection corrections together, preserving one initialized native owner
and one report source per controller.

[TR6](../testing-requests.md#tr6-qa-hid-lifecycle-and-notification-observations)
remains blocked on backend work and an observation harness. It needs initialization,
enumeration/open, source and actual HID report/lifecycle evidence; the demo trace
and successful Gainput `Init` do not provide those observations. The dependency
discards HID initialization's return code, so explicit startup-failure reporting
requires a contract change if selected.

Linux joystick QA uses HID disabled. Windows ownership/notification observations
belong to the [platform plan](long-term/platform-acceptance.md). Broader native
ownership requires a consumer case. Battery and touch/motion exposure remain separate
from the handoff's initial input/adaptive-trigger scope; use the
[native capability limits](../../projects/modules/platform/native-glfw/README.md#hid-capability-and-platform-scope)
when selecting extensions.

## Later work

[Mid-term work](mid-term/README.md) covers possible color emoji after Unicode
acceptance and measured render/text/timing optimization. Collect workload evidence
and settle compatibility/order semantics before introducing caches or batching.

[Long-term work](long-term/README.md) covers deferred platforms, manifests, Steam
API/Input and consumer-selected facilities. Steam transport and SDK-free preparation
have no short- or mid-term scheduling commitment.

The [Debug output console](unscheduled/debug-console.md) has selected requirements
but no implementation schedule. Resolve transport and platform prerequisites before
adding dependent integration work. Its native terminal is separate from the later
built-in graphical console.
