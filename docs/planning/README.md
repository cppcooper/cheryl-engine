# Planning catalogue

These horizons describe sequencing, not delivery dates. Work proceeds only when its
consumer requirements and prerequisites are settled.

| Horizon | Scope | Owning documents |
| --- | --- | --- |
| Short term | Audio integration, Unicode text and pending Linux asset/native acceptance | [Audio](audio-integration.md), [Unicode task](develop-review-and-development-plan.md#u11--unicode-text-layout-and-glyph-resources), [tile acceptance](develop-review-and-development-plan.md#u10--deterministic-tile-selection), [native lifetime task](develop-review-and-development-plan.md#native-input-lifetime-safety) |
| Mid term | Possible color-emoji follow-on after Unicode acceptance; measured optimization after workload evidence | [Text follow-on](develop-review-and-development-plan.md#u11-follow-on--color-emoji), [optimization](develop-review-and-development-plan.md#u12--measured-optimization-facilities) |
| Long term | Multi-platform acceptance, additional dungeon asset manifests, Steam API/Steam Input, optional input composition and consumer-selected extensions | [Long-term plan](long-term-plan.md), [deferred platform acceptance](platform-acceptance.md) |

The [Debug output console plan](debug-console.md) records the selected native-window,
test-output and lifetime requirements. Implementation scheduling remains unset;
the built-in graphical developer console is later work in the
[long-term plan](long-term-plan.md#other-engine-extensions).

Windows, macOS, Wayland and other platform testing are shelved. Their pending
coverage does not gate Linux short-term development; deferred coverage remains
unaccepted until those platforms are scheduled and exercised.

Gainput backend implementation remains deferred in the
[submodule handoff](../../extern/gainput/TODO.md). Cheryl retains the pinned Gainput
baseline while Engine asset/text work proceeds independently.

[todo.md](todo.md) summarizes unresolved engine facilities;
[asset-manifest-todo.md](asset-manifest-todo.md) tracks artwork metadata and owner
decisions needed before additional asset semantics can be encoded safely.

Current contracts and implemented adapter requirements belong in
[development and subject documentation](../README.md). Keep detailed checklists in
the plan that owns an active task; this catalogue only links to that work.
