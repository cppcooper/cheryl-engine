# Testing requests

This is the pending user-run acceptance queue for implemented work. Requests combine
coverage from multiple development cycles. Remove accepted requests and retain only
unresolved platforms or cases; the [roadmap](planning/develop-review-and-development-plan.md)
owns development progress, while subject guides retain reusable procedures and
meaningful coverage limits.

There are no active requests. Linux acceptance scope and remaining coverage limits
are recorded in the [text guide](assets/text-layout.md#native-acceptance-scope),
[audio guide](../projects/modules/audio/miniaudio/README.md#acceptance-boundaries),
[demo guide](../projects/apps/demo/README.md#repeating-native-qa) and
[UI guide](development/ui-adapters.md#repeating-linux-root-validation).
Reusable automated procedures remain in the
[Engine guide](development/architecture-validation.md#engine-asset-and-text-regressions)
and [controller guide](development/native-desktop-checks.md#linux-controller-automation).

HID backend work and TR6 are deferred together. The
[long-term acceptance plan](planning/long-term/platform-acceptance.md#deferred-hid-lifecycle-and-windows-notifications)
preserves Linux USB/Bluetooth lifecycle and Windows notification requirements,
blocked on backend corrections and an observation harness. Windows TR3–TR5 and
macOS/Wayland/other-platform prerequisites also remain in that plan.

Dedicated regressions not yet implemented remain in their owning development plans.
Restore requests during authorized testing-request maintenance when executable
targets and runnable observation paths are ready. A skipped or unavailable
observation leaves that coverage unresolved; successful automation does not establish
separate native QA observations.
