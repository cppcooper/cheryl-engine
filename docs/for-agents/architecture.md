# Architecture and design

Use this guide to decide how a requested capability or correction should fit the
existing system. Produce a source-backed recommendation that an implementation
can follow, including unresolved decisions and acceptance limits.

Read [shared workflow](workflow.md) for work modes, source authority, scope and
shared-tree boundaries. Use [implementation](implementation.md) after a design
has been selected, and [testing](testing.md) when defining its acceptance.
The engine's subject guides and declaring sources own its actual contracts;
this page supplies a design method rather than another engine architecture.

## Frame the problem

State the result the caller needs before choosing an API, class or module:

- Who performs the operation, and what useful result should they obtain?
- What application work, failure or dependency makes the current path inadequate?
- Which requirements are fixed, and which examples merely illustrate them?
- What must remain compatible, and what is outside this decision?

Separate the requested outcome from a proposed mechanism. An example involving a
book, HUD or loader does not by itself require an engine-owned book, HUD or loader
policy. Identify the reusable responsibility and the application choices before
deciding where either belongs.

## Establish the current and intended contracts

Trace the relevant public entry point through its implementation and cooperating
owners. Read the configuration and relevant tests where they establish selection,
lifetime or failure behavior. Expand investigation to resolve a consequential
uncertainty; avoid collecting an unrelated source inventory.

Keep these distinctions explicit:

| Evidence | Record |
| --- | --- |
| Observed implementation | What the current source and configuration actually do, including rejection and cleanup. |
| Documented intent | The requirements or rationale in the owning guide or plan; identify any mismatch with source. |
| Verified behavior | The cases actually observed, with their configuration and limits. |
| Open question | The assumption, missing evidence and consequence if it proves false. |

Look for an existing operation that already supplies the requested responsibility.
Check its convenience layers and lifetime authority before proposing another
registry, dispatcher, fallback, ownership wrapper or application workaround.
Distinguish a confirmed bug, an absent integration and an optional convenience;
they can require different decisions.

## Compare viable approaches

Consider keeping the current contract, extending an existing owner, and adding a
new boundary where those are meaningful alternatives. Explain the consequences
that affect the caller and maintainer, rather than presenting an inventory of
possible abstractions.

Compare the applicable tradeoffs:

- Application setup and the ordinary path, including sensible existing defaults.
- Ownership, dependency direction and the ability to use a component independently.
- Lifetime and shutdown obligations, retained data and borrowed access.
- Execution owners, synchronization, publication and progress during waits.
- Failure visibility, recovery, cancellation and preservation of useful state.
- Compatibility, migration effort and the cost of maintaining multiple paths.
- Evidence and fixtures needed to establish the claimed behavior.

Evaluate an interface by its architectural purpose and contract. Do not remove it
because it has no callers or few callers. Determine which extension, replacement,
independent consumer or supported responsibility it exists to serve.

Recommend the approach that addresses the actual requirements, with the material
tradeoff and rationale. Do not imply that a broader redesign is necessary merely
because it would simplify the proposed local implementation.

## Specify the boundary precisely

Describe a concrete caller sequence and enough API shape to expose the important
decisions. A short example can reveal missing initialization, result adoption or
cleanup that a type diagram hides. Identify proposed APIs as proposed.

| Contract question | Resolve when applicable |
| --- | --- |
| Inputs and outputs | Valid values, units, defaults, identifiers and returned ownership. |
| Ownership and lifetime | Owners, borrowed targets, retained snapshots, invalidation and destruction order. |
| Execution | Allowed callers, owner threads, synchronization and whether an operation queues, blocks or runs inline. |
| State and publication | Acceptance versus completion, visibility boundaries, ordering and replacement/coalescing behavior. |
| Failures | Synchronous exceptions, asynchronous results, cancellation, partial effects, retry and preserved state. |
| Capability limits | Observable unsupported cases and the guarantees supplied by optional integrations. |

Trace shutdown as carefully as ordinary success. Determine which producers stop,
which accepted users must finish, who continues servicing dependencies, and when
borrowed objects or native domains can disappear. Retaining a handle does not
automatically retain every resource needed to use it.

## Check compatibility and acceptance

Identify affected public declarations, headers, link targets, saved values and
application sequences. Separate source compatibility, binary compatibility and
runtime behavior where they differ. Explain a required migration without silently
changing an established contract to make a new example convenient.

For build, tooling, SDK or CI boundaries, follow
[producer-to-consumer analysis](implementation.md#trace-build-tooling-sdk-and-ci-changes).
Check the affected configurations rather than assuming one source build answers
installed, relocated or packaged consumption.

Define acceptance in terms of observable behavior: the normal caller outcome,
material rejection/failure cases, lifecycle boundaries and relevant configurations.
Identify existing coverage that can be reused and the specific missing cases or
fixtures. Use the [testing guide](testing.md) to distinguish proposed coverage
from checks that actually ran.

## Hand off a decision

Give the next phase a concise account of:

- The selected contract and why it addresses the caller's problem.
- The responsible owner, concrete call sequence and affected boundaries.
- Material alternatives rejected and the tradeoff that decided between them.
- Source evidence, assumptions and decisions that remain unresolved.
- A coherent implementation sequence and its dependency checkpoints.
- Required acceptance, observed evidence and outstanding limits.

Keep open decisions visible. A design recommendation does not establish that the
implementation exists or that its behavior has been accepted. Use the
[shared workflow](workflow.md) for progression and handoff boundaries, and the
[documentation workflow](documentation.md) when maintaining durable design or
subject documentation is part of the task.
