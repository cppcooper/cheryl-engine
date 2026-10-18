# Implementation, refactoring and tooling

Use this guide to turn an agreed outcome or design into a coherent development
unit. It covers implementation planning, producer-to-consumer changes and Cheryl
code conventions. The [shared workflow](workflow.md) owns scope, source authority,
authorization, repository safety and shared-work boundaries.

Use [architecture and design](architecture.md) for unresolved contract decisions,
[testing](testing.md) for test work and verification, [Git](git.md) for commits,
and [documentation](documentation.md) for a documentation phase.

## Prepare a coherent development unit

Before nontrivial implementation, establish a concise plan containing:

- The requested outcome and the behavior or contract being changed.
- Coherent tasks that can be completed and reviewed together.
- Risky assumptions and the evidence needed to resolve them.
- Dependency boundaries and any required decision checkpoints.

Keep the plan in the working context unless a persistent planning artifact was
explicitly requested. Resolve foundational decisions and uncertain boundaries
before accumulating dependent code. Follow the plan within its approved scope;
revise it when implementation reveals material new information.

Identify the current owner of the behavior and trace its callers and dependencies
far enough to understand the change. Read the relevant implementation, tests and
configuration rather than treating a public declaration as its complete behavior.
Use the [architecture guide](architecture.md#specify-the-boundary-precisely) when
ownership, execution, failure or compatibility needs a new decision.

## Implement the complete behavior

Make the approved design concrete through its relevant lifecycle: construction,
ordinary use, state transitions, failure, cleanup and the affected consumer path.
Keep application policy and reusable responsibilities with their selected owners.
Preserve the established contract outside the requested change.

Necessary investigation, affected test maintenance and focused regression coverage
belong with the behavior being implemented. Follow the [testing guide](testing.md)
for their design, execution and evidence limits. Address material failure paths
as well as the straightforward success case.

Add essential comments and runtime logging required by the implementation at the
relevant operation. Keep broader comment cleanup, unrelated logging improvements
and explanatory documentation with their own objectives. The
[documentation workflow](documentation.md) owns synchronization and maintenance.

## Trace build, tooling, SDK and CI changes

### Establish the producer and consumer contract

Before changing behavior, identify the affected contract, its owner and the path
from producer to consumer. Follow the portions relevant to the task through:

1. Artifact creation and the inputs/configuration that select it.
2. Installation and packaging, including dependency ownership.
3. Relocation and runtime discovery/loading.
4. Cache reuse and invalidation at the affected boundary.
5. Workflow activation and the prerequisites that make the path run.

An artifact being created does not establish that it was installed, shipped,
found after relocation or used by the intended consumer. Inspect only the affected
path; broad inventories do not replace tracing that contract.

### Extend the existing policy owner

Locate the existing authority for each policy before adding another implementation.
Prefer extending that owner over duplicating policy in a wrapper, classifier,
cache rule, fallback or compatibility branch.

When CMake, Python or workflow code must express the same policy, keep their
semantics aligned and cover their agreement with focused checks. Check both the
decision and the context in which it is evaluated; identical-looking expressions
can receive different inputs at configuration, installation or script execution.

### Select the affected execution contexts

Choose coverage according to the changed boundary, including relevant differences:

| Context | Question |
| --- | --- |
| Project configuration versus generated CMake install/script execution | Are values, paths and available targets valid at the time the code runs? |
| Source versus installed SDK consumption | Does the installed consumer receive the promised headers, targets and dependencies? |
| Static versus shared linkage | Are dependency closure and runtime loading correct for the selected linkage? |
| Linux versus Windows | Are platform paths, binaries and native requirements handled by the responsible owner? |
| Cold versus warm caches | Can fresh production and retained artifacts both follow the intended policy? |
| Default-branch workflow prerequisites | Will the intended event, branch and upstream artifacts actually activate the changed workflow? |

Use the relevant subset; do not run the entire matrix for every edit. Follow
[testing and verification](testing.md) for permitted checks, resource use and
reuse of compatible builds/caches.

### Preserve the contract when repairing a check

Trace dependency ownership before changing bundling rules or system-library
exceptions. Distinguish correcting a verifier from changing what the SDK promises
to supply. A failing check is evidence to investigate, not permission to weaken
the producer or consumer contract.

For repeated failures, revisit the common assumption before growing special cases.
Remove superseded paths within the requested unit; keep a broader redesign as
separate discovered work.

## Follow Cheryl conventions

- Follow the repository's existing style and `.clang-format`.
- Put data members above methods when organizing class declarations.
- Keep unit-test names short and intuitive.
- Exercise behavior without intentionally emitting compiler warnings in normal
  test builds. Consume results that matter to an assertion; explicitly discard an
  irrelevant `[[nodiscard]]` result with `static_cast<void>(...)`, including inside
  exception assertions. Apply the same discipline to other intentional diagnostic
  triggers.
- Test compiler diagnostics in isolated probes that capture and assert the expected
  diagnostic. Keep suppression scoped to that construct and compiler. Preserve API
  attributes and target/project warning settings rather than weakening them to
  accommodate a test.
- Evaluate interface changes through their
  [architectural purpose and contract](architecture.md#compare-viable-approaches),
  including supported extension and replacement responsibilities.

## Review, verify and commit the unit

Review the resulting behavior and consumer path against the agreed outcome.
Check that a local repair has not shifted ownership, changed unrelated guarantees
or introduced a second policy authority. Use the smallest relevant permitted
checks and maintain the directly affected coverage through the
[testing guide](testing.md).

Complete and commit coherent development units independently after appropriate
permitted checks, following the [Git guide](git.md). State outstanding executable
acceptance explicitly; a commit does not establish that an unrun native, installed
or runtime path has succeeded.

At handoff, describe what changed and why, the relevant contract, evidence actually
observed, and remaining verification or decisions. Preserve discovered testing
needs for the [testing-request process](testing-requests.md), and use the
[documentation workflow](documentation.md) for any authorized documentation phase.
