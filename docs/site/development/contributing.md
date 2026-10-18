# Contributing

Start with the guide for the subsystem you are changing. Choose the owning library
before adding code: neutral contracts belong in Engine, runtime integration SDKs
in their modules, optional Engine helpers in support targets, and game mechanics
in the application. [Write a module](modules.md) explains when a new owner is
useful and how to compose it.

Keep a change focused on its intended behavior. Preserve unrelated work in this
shared checkout and avoid changing interfaces or formatting nearby code merely
because it is convenient. Apply the [C++ style](code-style.md) to affected code.
Pinned dependencies retain their own licenses and upstream conventions.

```{toctree}
:hidden:
:titlesonly:

C++ code style <code-style>
```

## First change

1. Find the subsystem's [Engine Systems entry](../engine-systems/index.md) and the
   implementation owner; read the relevant API/subject contract before editing.
2. State the observable behavior being changed, the callers affected and any
   compatibility or ownership constraint. Use the existing owner unless a new
   module meets the [boundary criteria](modules.md#module-boundary-criteria).
3. Make the focused change and its directly affected regression coverage. Use the
   [check selector](testing.md#choose-checks-for-a-change) to choose the smallest
   checks that establish the changed contract.
4. Reconcile affected API/manual contracts during documentation maintenance, then
   hand off the behavior, actual checks and outstanding acceptance separately.

## Validation

Add behavior checks under the owner they exercise. Engine checks use controlled
adapters; integration checks belong with the selected module. Compile public
headers through the owner's consumer probes when a change affects consumption.
Use [Testing and validation](testing.md) to batch targets and avoid duplicate aggregate
runs. Native pixels, sound, controller reports and platform services require their
own observation procedures.

When reporting a change, explain the resulting behavior, why it belongs in that
owner and the checks performed. Identify unresolved failures and missing coverage
with their next action. Agent execution and commit rules live in
[AGENTS.md](../../../AGENTS.md).

## Documentation and planning

During an authorized documentation-maintenance phase, update the authoritative
API or subject guide when contracts change, including
ownership, valid threads, units, preconditions and failure behavior. Keep build
instructions in the [build guide](building.md), runner selection in the
[testing guide](testing.md), and integration-specific details with their module.

Plans describe unresolved work, prerequisites, dependency boundaries and acceptance.
Keep an active task's checklist in one owning plan and link it from the roadmap.
Preserve useful decisions before removing completed plans; implementation history
belongs in Git. Maintain [testing requests](../../testing-requests.md) only when that
maintenance is authorized: carry outstanding requirements in the phase handoff
until then, and remove accepted requests after reconciling revision and coverage.
Documentation audit reports findings without editing; maintenance implements the
selected corrections. Test design, implementation and execution remain separate
work modes, and listing build/test commands does not authorize agent execution.
