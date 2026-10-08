# Contributing

Start with the [architecture overview](../runtime/runtime-architecture.md) and the
guide for the subsystem you are changing. Choose the owning library before adding
code: neutral contracts belong in Engine, SDK implementations in their modules,
and game mechanics in the application. The [module guide](modules.md) explains
when a new owner is useful and how to compose it.

Keep a change focused on its intended behavior. Preserve unrelated work in this
shared checkout and avoid changing interfaces or formatting nearby code merely
because it is convenient. Apply the [C++ style](code-style.md) to affected code.
Pinned dependencies retain their own licenses and upstream conventions.

## Validation

Add behavior checks under the owner they exercise. Engine checks use controlled
adapters; integration checks belong with the selected module. Compile public
headers through the owner's consumer probes when a change affects consumption.
Use [test selection](testing.md) to batch targets and avoid duplicate aggregate
runs. Native pixels, sound, controller reports and platform services require their
own observation procedures.

When reporting a change, explain the resulting behavior, why it belongs in that
owner and the checks performed. Identify unresolved failures and missing coverage
with their next action. Agent execution and commit rules live in
[AGENTS.md](../../AGENTS.md).

## Documentation and planning

Update the authoritative API or subject guide when contracts change, including
ownership, valid threads, units, preconditions and failure behavior. Keep build
instructions in the [build guide](building.md), runner selection in the
[test guide](testing.md), and integration-specific details with their module.

Plans describe unresolved work, prerequisites, dependency boundaries and acceptance.
Keep an active task's checklist in one owning plan and link it from the roadmap.
Preserve useful decisions before removing completed plans; implementation history
belongs in Git. Add outstanding user-run validation to
[testing requests](../testing-requests.md) and remove accepted requests after
reconciling the reported revision and coverage.
