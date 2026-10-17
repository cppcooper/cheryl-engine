# Cheryl Engine Agent Instructions

## General instructions

### Work modes and progression

Each task has one primary work mode:

- Review and planning
- Investigation and research
- Architecture and design
- Implementation, refactoring, and tooling
- Test design
- Test implementation
- Test execution
- Verification
- Documentation audit
- Documentation maintenance

Keep independent work objectives separated by mode. A request for review or investigation does not authorize implementation merely because a fix becomes apparent.

A request to implement or fix behavior includes necessary investigation, updates to affected tests, focused regression coverage where useful, and verification permitted below. These supporting activities belong to the same development unit. Independent test design, broader verification campaigns, and documentation work remain separate objectives unless authorized.

User-directed progression is the default: complete the requested objective and stop before starting an independent objective. When a bounded sequence would benefit from plan-directed progression, propose its phases and decision checkpoints before adopting it. An approved plan may authorize progression across modes without further permission for each coherent subtask.

Stop when:

- The requested scope is complete or an approved plan reaches a mandatory decision checkpoint.
- A material requirement, architectural decision, or invalidated assumption requires user direction.
- A necessary operation or scope expansion is not already authorized.
- Continuing would depend on unfinished user work or risk modifying protected changes.
- A blocking condition prevents safe or correct completion.

A completed subtask or necessary supporting activity is not itself a stopping point. Continue authorized work that can safely proceed, including independent work unaffected by a blocked portion. Report any boundary, its implications, and the recommended next action.

At handoff, distinguish implemented changes, observed verification, and outstanding or deferred work. Completing an implementation phase does not establish executable acceptance or complete a broader task with required work remaining. Optional improvements and advisory documentation checkpoints neither block completion nor authorize further work.

### Source authority and context

Prefer source code, configuration, and actual implementation when determining the repository's current state and behavior.

Use documentation to understand intended architecture, requirements, design rationale, historical decisions, and planned work. Documentation may describe previous, intended, or partially implemented behavior.

Verify documentation's claims about current implementation against the relevant code. When implementation and documented intent disagree, distinguish the observed state from the intended state rather than automatically treating either as correct.

Acquire context purposefully. Inspect the code, documentation, history, and dependencies relevant to the current objective. Expand investigation when necessary rather than loading unrelated project context by default.

Prefer concise phase handoffs containing established decisions, relevant constraints, outstanding work, and affected contracts. Do not create persistent handoff or planning documents unless explicitly requested.

## Repository safety

- Do not push commits or branches to any remote unless explicitly requested.
- Do not build or compile the project or its dependencies unless explicitly authorized by the user's request or an approved plan.
- Preserve active user runs and their outputs. Do not change their configuration, clean their build directories, or invalidate their caches as a routine preparation step.
- Do not perform opportunistic refactoring, cleanup, formatting, or API changes merely because they are convenient while working nearby.
- Report related work outside the current development unit as discovered work. Resolve necessary dependencies within the authorized scope; use the progression boundaries above when further work requires user direction.

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

## Shared working tree

The repository is a shared human/agent working tree. Pre-existing uncommitted changes may represent deliberate work in progress and may be incomplete, temporarily inconsistent, non-functional, transitional, preparatory for later refactoring, or difficult to reason about safely.

Before modifying affected code:

- Inspect the current Git status and relevant pre-existing diffs.
- Treat pre-existing user changes as authoritative work in progress. Do not revert, overwrite, normalize, reformat, or replace them merely to simplify the task.
- Do not assume pre-existing changes form a valid, complete, or stable implementation baseline.
- Do not build new task work on top of pre-existing unfinished user work merely because its current behavior is understandable.
- Avoid introducing new dependencies on unfinished user work, propagating its current structure into additional code, constraining its future evolution, or otherwise increasing the amount of work that would need to change when the user's work is completed.

A file containing user changes is not automatically off-limits. Work with it only when the requested change is independent of the unfinished work, or when the change can be made without increasing the dependency surface, constraining future evolution, or increasing the likely scope of subsequent refactoring.

If completing the requested work would require treating unfinished user work as a stable foundation, extending its current design into other code, or making additional code depend on it, treat that as a development boundary.

When such a boundary is encountered:

- Preserve the existing user work unchanged where possible.
- Stop at that boundary.
- Continue with independent planned work only when it is clearly unaffected by, and does not depend on, the blocked work.
- Clearly identify what cannot be completed safely and why.

When committing:

- Commit only task-related changes that can be cleanly separated from pre-existing user work.
- Do not include unrelated or unfinished user work in an agent-created commit.
- If task-related work and pre-existing user work cannot be separated safely, leave the overlapping work uncommitted rather than rewriting the user's work to manufacture a clean commit.

## Planning and execution

Before beginning non-trivial implementation work, establish a concise plan containing coherent tasks, risky assumptions, dependency boundaries, and any required decision checkpoints. Keep it in the working context unless a persistent planning artifact is explicitly requested.

Resolve foundational decisions and uncertain boundaries before accumulating dependent work. Follow the plan within its authorized scope, revising it when implementation reveals material new information. A plan does not authorize operations or independent work outside its approved scope.

Complete and commit coherent development units independently after appropriate permitted checks. State any outstanding executable acceptance rather than implying that a committed implementation has been fully verified.

### Build, tooling, SDK, and CI changes

- Identify the affected contract, its owner, and the path from producer to consumer before changing behavior. Include relevant artifact creation, installation, packaging, relocation, runtime loading, cache reuse, and workflow activation boundaries; inspect only the portions affected by the task.
- Locate the existing authority for each policy before adding another implementation. Prefer extending that owner over duplicating policy in another wrapper, classifier, cache rule, fallback, or compatibility branch. When CMake, Python, or workflow code must express the same policy, keep their semantics aligned and cover their agreement with focused checks.
- Review the execution contexts affected by the change. Examples include project configuration versus generated CMake install or script execution, source versus installed SDK consumption, static versus shared linkage, Linux versus Windows, cold versus warm caches, and default-branch workflow prerequisites. Select relevant coverage; do not run the entire matrix for every edit.
- Preserve the intended contract when correcting a check. Trace dependency ownership before changing bundling rules or system-library exceptions. Distinguish correcting a verifier from changing what the SDK promises to supply.
- For repeated failures, revisit the shared assumption or boundary before adding another local exception. Remove superseded paths when that is part of the requested change; report larger simplification work separately rather than turning a fix into an unrequested redesign.

## Testing requests

Testing-request documentation is maintained only when explicitly authorized. Do not update `docs/testing-requests.md` automatically during implementation, refactoring, verification, or other non-documentation phases.

Preserve discovered testing requirements in the task handoff until a testing-request maintenance phase is authorized. At that point, reconcile accumulated requirements against existing requests.

These rules govern testing-request documentation. They do not defer directly related test maintenance or permitted verification from an implementation task.

During authorized testing-request maintenance:

- Maintain `docs/testing-requests.md` as the user-facing queue of testing needed for implemented work. Reconcile accumulated testing needs with the current source, targets, configuration, reported results, and required coverage.
- Keep a summary table near the top with exactly three columns: **TR#**, **Short description / title**, and **Long description (plain language)**. Give every active request one row, with its `TR<number>` linked directly to its detailed section/heading. Include the same ID in that heading. Preserve request IDs when updating requests; do not renumber remaining requests or reuse retired IDs.
- Prefix each summary-table **Short description / title** cell with **Automated:** or **QA:**, matching the request type in its detailed section.
- Write each table's long description for a reader unfamiliar with engine or testing internals: explain what will be checked, why it matters, and any setup requirement or blocker affecting whether they can run it. Explain necessary jargon; keep exact commands and detailed acceptance criteria in the linked section.
- Aggregate deferred testing across development cycles. Extend an existing request when it covers the same behavior and configuration; avoid duplicate case runs through owner and aggregate runners, and batch needed build targets into as few invocations as practical.
- Label each request as **Automated** or **QA**, with its platform, prerequisites, readiness and acceptance scope. Keep development progress in the owning plan rather than duplicating its macro-task checklist here.
- For each automated request, provide a runnable fenced command block. Include required CMake configuration, explicit build targets, environment opt-ins, working directory and test selection. State when a matching existing build can be reused.
- Command blocks must establish their working directory explicitly, including QA launch blocks. Commands launched from a document may inherit that document's directory; locate the checkout root rather than assuming the shell already runs there, and preserve the caller's starting directory.
- User-run command-line CMake configuration must explicitly select Ninja with `-G Ninja`. Build and test commands use that configured build directory.
- For each QA request, provide a brief detailed summary and concise requirement bullets covering setup, actions, expected observations and required platform/runtime modes.
- Mark requests blocked when fixtures, hardware or an observation harness are unavailable, and identify the next action. Do not invent commands or count skipped, unselected or device-free checks as the missing acceptance.
- When the user reports results, reconcile requests and owning plans during the next authorized maintenance phase against the actual tested revision, configuration, platforms and coverage. Remove accepted requests, preserve unresolved portions and repair affected links; retain durable coverage limits in the relevant subject guide rather than appending routine execution logs.

Submitting a testing request does not authorize operations that require explicit permission under Verification and resource use.

## Documentation discipline

Reading documentation is permitted in any work mode when relevant to understanding requirements, intent, constraints, or history. Prefer the relevant source code when establishing current behavior, and consult the relevant roadmap or subject document for direction rather than loading the entire planning folder.

Documentation auditing and maintenance are separate work modes. Do not create, update, reorganize, or delete documentation during other work modes unless explicitly requested.

Essential code comments and runtime logging directly required by the implementation are permitted. Do not perform general comment cleanup, logging improvements, or explanatory documentation work opportunistically.

Defer documentation synchronization until a dedicated maintenance phase. Prefer stabilizing related implementation work before reconciling documentation.

Recommend a documentation checkpoint at a meaningful phase boundary when architectural contracts stabilize, substantial implementation changes accumulate, or documentation is likely to have become materially inaccurate.

Documentation checkpoints are advisory. Do not conduct a documentation audit or begin maintenance merely to make the recommendation. The user decides whether to continue engineering work or authorize documentation maintenance unless the transition was already approved.

### Documentation audit

Inspect the relevant implementation and documentation to identify inaccurate, obsolete, missing, duplicated, or unnecessary information.

Report findings and recommended changes without modifying documentation. Distinguish documentation describing intended behavior from documentation incorrectly describing the current implementation.

### Documentation maintenance

Documentation should help a reader understand the current system, its intended direction, or the work required to reach that direction.

Prefer updating and consolidating existing authoritative documents over creating new ones. Remove obsolete, duplicate, or superseded documentation when its useful information has been preserved. Do not expand the document collection merely to record intermediate work, reasoning, or implementation progress.

Apply these rules to affected documents across `docs/` during authorized maintenance:

- Describe current contracts in the present tense. Keep plans focused on the target state, unresolved work, prerequisites, ownership/dependency constraints, ordered steps, discovery boundaries and acceptance criteria.
- Keep one concise progress checklist for each active macro task in the plan that owns its details. Reconcile accumulated subtask progress during maintenance, retaining checked subtasks while the macro task remains open. Keep future macro tasks at roadmap detail until they become active; link to the owning checklist instead of maintaining duplicate progress lists.
- Remove completed macro-task narratives and superseded alternatives. Document resulting durable behavior in its authoritative subject document, and link to it instead of duplicating contracts or source inventories.
- Preserve unique requirements, decisions and rationale that still guide implementation. Before shortening or deleting a plan, verify that each remains available in the surviving text or a linked authoritative document. Do not replace relevant detail with a vague summary or a pointer to Git alone.
- Git commits and diffs hold implementation history. A brief decision/history summary with specific Git references is useful when it explains the current direction or saves a future reader substantial reconstruction; do not reproduce the task-by-task journal.
- Do not append routine successful build/test/check reports, pass counts, transcripts or toolchain snapshots unless explicitly requested. Retain reusable validation procedures and meaningful coverage limits. Distinguish source completion from executable acceptance; a successful exit with skipped or unselected checks does not establish the missing coverage.
- Record unresolved failures, unavailable prerequisites and environment/compatibility limits only to the extent they affect future work, with the required next action. Remove resolved blockers unless they describe an enduring constraint.
- Reconcile affected plans and current-state documentation against accumulated completed work. When a macro task completes, remove its task section and checklist after preserving any reusable procedure, durable contract or still-relevant decision rationale and repairing inbound links. Delete a one-purpose plan when no unresolved work remains. Do not move its execution log elsewhere merely to empty the plan.

Keep a paragraph when it explains how something works, how it should work, how to get there, or a decision/constraint needed to do so correctly. Concision must preserve that information.

## Git

For commits created by the agent, use:

- `user.name = cppcooper`
- `user.email = cppcooper@users.noreply.github.com`

Apply this identity per Git command using `git -c`. Do not modify the user's global Git configuration or the repository's local Git configuration.

Commit messages must follow the project's convention and begin with a third-person singular present-tense verb, such as:

- `Adds`
- `Updates`
- `Revises`
- `Deletes`
- `Fixes`
- `Requires`

Use detailed commit messages appropriate to the completed development unit.

Preserve the individual commit series. Do not squash, amend, or otherwise rewrite completed commits unless explicitly requested.

Do not generate mailbox patches unless explicitly requested.

## Cheryl conventions

- Follow the repository's existing style and `.clang-format`.
- When organizing class declarations, place data members above methods.
- Keep unit-test names short and intuitive rather than long and descriptive.
- Tests must exercise behavior without intentionally emitting compiler warnings in normal builds. Consume results when they matter to the assertion; explicitly discard irrelevant `[[nodiscard]]` results with `static_cast<void>(...)`, including inside exception assertions. Apply the same warning discipline to other intentional diagnostic triggers.
- Test compiler diagnostics themselves through isolated probes that capture and assert the expected diagnostic. Keep any required diagnostic suppression narrowly scoped to the tested construct and compiler; do not remove API attributes or disable warnings across a target/project to accommodate a test.
- Do not use "no callers" or "few callers" as grounds for removing an interface. Evaluate interfaces according to their architectural purpose and contract.
