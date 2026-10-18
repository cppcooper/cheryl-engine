# Testing and verification

Use this guide for test design, test implementation, test execution and
verification. The [workflow](workflow.md) owns task scope, authorization and
progression between independent objectives. [Implementation](implementation.md)
owns code conventions, test naming and compiler-warning discipline.

The [testing and validation guide](../site/development/testing.md) maps repository
runners and procedures. Maintaining the user-facing testing queue is a separate
objective governed by [testing-request maintenance](testing-requests.md).

## Choose the objective and evidence

| Objective | Work | Handoff |
| --- | --- | --- |
| Test design | Read the contract, implementation and existing coverage. Identify observable outcomes, relevant failure paths, prerequisites and the smallest cases that distinguish correct behavior. | Proposed cases, expected results, fixtures and coverage limits. A design is not implemented or executed coverage. |
| Test implementation | Update affected cases and shared fixtures; add focused regression coverage where needed. Follow the implementation guide's naming and warning rules. | Cases changed and the behavior they exercise, with any permitted checks actually run. Written cases alone do not establish acceptance. |
| Test execution | Inspect the selected runner and its subprocesses, confirm the authorized operation and compatible configuration, then run the focused selection. | Actual revision, configuration, selection, results, skips and unavailable prerequisites. |
| Verification | Trace the affected contract through its owner and consumers, choose appropriate evidence, and compare observed results with the required outcome. | Confirmed behavior or failure, inferred cause, and remaining acceptance needed. |

For supporting regression work within an implementation, apply the workflow's
existing authorization and progression rules. Keep supporting work within that
development unit; an independent testing campaign follows the workflow's task
boundaries.

## Select relevant coverage

Choose cases from the changed contract and its failure/ownership boundaries,
then inspect existing owner and aggregate runners before selecting targets.
Prefer an existing case or fixture when it exercises the required behavior.
Trace the operation far enough to identify what it actually checks; an aggregate
name alone does not establish which cases run.

For build, tooling, SDK and CI work, use the producer-to-consumer analysis and
[affected execution contexts](implementation.md#select-the-affected-execution-contexts)
from the implementation guide to choose relevant configurations and consumers.
Tie each selected check to the boundary it verifies.

A mocked command or fixture can establish orchestration without establishing
that the generated command succeeds or its output can be consumed. Select
artifact, relocation, runtime or device evidence when that is the claim being
verified, subject to the authorization rules below.

## Verification and resource use

- Run the smallest relevant permitted checks before handing off an implementation. Update affected tests with the behavior they cover, and add focused regression coverage when existing checks do not adequately exercise a material change.
- Static analysis and focused script or tooling tests using mocks or disposable fixtures are permitted without separate authorization when they do not compile project or dependency code, run native project executables, install or download tools or dependencies, generate full documentation, or access devices or external services. CMake script and configuration probes that enable no compiled languages may qualify.
- Inspect an unfamiliar check before executing it, including the subprocesses it launches. A Python filename or a description such as "tooling check" does not establish that a check is lightweight. Configuration that invokes compiler or linker probes requires build authorization.
- Project or dependency builds, native test execution, full documentation generation, dependency or tool downloads or installation, and tests using devices or external services require explicit authorization. Existing authorization persists for relevant fixes and reruns within its scope. When additional authorization is necessary to establish the requested outcome, identify the missing verification and request it together rather than waiting for an unrelated blocker.
- Batch related checks and reuse compatible builds and caches. After fixing a failed check, rerun the affected checks when permitted. Broaden or repeat successful checks only when changed inputs, failures, or unresolved concerns justify it.
- Share machine resources during authorized heavy work by bounding agent-run parallelism or reducing process priority as needed. Commands supplied for the user run at normal priority with available parallelism; do not add low-priority wrappers or single-job limits to user-run commands.
- Test behavior and contracts rather than mirroring implementation details. Use mocks to check orchestration, and distinguish that coverage from executing the generated commands or consuming the resulting artifacts.
- Report what actually ran and what remains unverified. Static checks, mocked tests, and successful configuration do not establish native build, link, relocation, or runtime success. Distinguish confirmed failures, inferred causes, and fixes awaiting executable acceptance.

### Local Windows tooling

- On this workstation, reusable Windows tooling lives in `/mnt/projects/cheryl-windows-ci/`. Check it before downloading tools, creating another Wine environment, or reporting that Windows tooling is unavailable. If it is absent on another host, look for compatible installed tools there.
- The `tools/` directory contains Windows Python, CMake, Ninja, Git, ccache, and vcpkg. Read `env.sh` and `run.sh` for current paths, environment settings, and supported phases before using them; inspect the selected phase before execution. MSVC and the Windows SDK are installed under `/opt/msvc/`.
- Preserve `tools/`, `downloads/`, and `wine-prefix/` across sessions. Store additional reusable tools and isolated Wine prefixes under this disk-backed tooling root, preserving existing versions and active work.
- `/tmp` is memory-backed on this workstation. During RAM cleanup, preserve reusable tools and cached installers; move them to disk-backed storage when needed. Stop idle agent-owned processes and remove disposable test outputs within the authorized scope.
- These locations do not authorize builds, native tests, downloads, or changes to an active environment. The existing verification and shared-work restrictions still apply.

## Report the result

State what changed, which checks actually ran, and what remains unverified.
Include the tested revision and relevant configuration/selection when they affect
the conclusion. Separate an unexecuted case, a skipped case, a confirmed failure,
an inferred cause and a fix awaiting executable acceptance.

For deferred requirements and queue maintenance, follow
[testing-request maintenance](testing-requests.md). Use the
[documentation workflow](documentation.md) for other authorized documentation
work. Follow [Git](git.md) for a coherent development-unit commit and its
verification limits.
