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

Keep independent work objectives separated by mode. Do not automatically transition into another mode merely because related work is discovered or the next phase seems obvious.

Reading code, inspecting dependencies, checking Git state, and performing other necessary supporting operations do not constitute a mode transition. Keep supporting work proportional to the primary objective.

#### Progression control

User-directed progression is the default. Complete the requested work mode and stop at its boundary unless further work has been explicitly authorized.

When a bounded sequence of work is predictable, reasonably well-defined, and likely to benefit from fewer interruptions, recommend plan-directed progression. Explain the proposed phases and decision checkpoints. Do not adopt the sequence without user approval.

An approved plan may authorize progression across work modes. Complete each phase before advancing to the next, preserving their boundaries and respecting existing authorization restrictions.

Neither progression model requires permission for every coherent subtask within an already authorized phase.

#### Boundary handling

A boundary is a point where continuing work may require a new decision, a change in scope, or a transition between work modes. Completing a subtask or encountering a dependency does not automatically constitute a stopping point.

**Continue without interruption when:**

- Work remains within the authorized mode, scope, and approved plan.
- A necessary dependency can be resolved without exceeding that authorization or introducing an unresolved architectural decision.
- The next subtask requires no additional user decision.
- Supporting operations are necessary to complete the authorized work.

**Stop at the boundary when:**

- The requested phase or scope is complete under user-directed progression.
- An approved plan reaches a mandatory decision checkpoint.
- Continuing requires an unauthorized work mode, scope expansion, or operation.
- An unresolved architectural decision, requirement ambiguity, or materially invalidated assumption requires user direction.
- Continuing would depend on unfinished user work or risk modifying protected changes in the shared working tree.
- A blocking condition prevents safe or correct completion.

When a boundary is encountered, complete any remaining independent, authorized work that can safely proceed. Report the boundary, its implications, and the recommended next action.

Advisory checkpoints, including documentation-maintenance recommendations, do not require stopping or independently authorize additional work.

During plan-directed progression, advance through approved phases without interruption unless a mandatory decision checkpoint or unexpected blocking condition is encountered.

Existing repository safety and authorization restrictions always take precedence.

#### Phase completion and task completion

A work phase and the overall development task are distinct.

Completing a phase means its authorized objectives have been satisfied or its remaining work has been explicitly identified as blocked or deferred. It does not imply that the broader development task is complete.

At phase completion:

- Summarize completed work and any outstanding, blocked, or deferred work.
- Identify the recommended next phase when applicable.
- Recommend documentation maintenance when accumulated changes warrant it, without initiating maintenance automatically.
- Stop under user-directed progression, or continue according to an approved plan-directed sequence.

Do not declare the overall task complete while known required work remains unfinished. Do not treat optional improvements or deferred documentation maintenance as blockers unless they are explicitly required by the task's acceptance criteria.

### Source authority and context

Prefer source code, configuration, and actual implementation when determining the repository's current state and behavior.

Use documentation to understand intended architecture, requirements, design rationale, historical decisions, and planned work. Documentation may describe previous, intended, or partially implemented behavior.

Verify documentation's claims about current implementation against the relevant code. When implementation and documented intent disagree, distinguish the observed state from the intended state rather than automatically treating either as correct.

Acquire context purposefully. Inspect the code, documentation, history, and dependencies relevant to the current objective. Expand investigation when necessary rather than loading unrelated project context by default.

Prefer concise phase handoffs containing established decisions, relevant constraints, outstanding work, and affected contracts. Do not create persistent handoff or planning documents unless explicitly requested.

## Repository safety

- Do not push commits or branches to any remote unless explicitly requested.
- Do not build or compile the project unless explicitly requested.
- Avoid requesting permission to build or test until reaching a blocking point.
- When building is permitted, make an attempt to avoid running consecutive builds.
- Do not run tests unless explicitly requested.
- When testing is permitted, make an attempt to avoid running consecutive tests.
- Instead of consecutive builds and tests, make efforts to achieve the same end result in as few steps as possible.
- If consecutive agent-run builds must be used, then reduce the process priority and or thread count involved to avoid melting the CPU.
- Resource-sharing limits apply to agent-run work. Commands supplied for the user run at normal priority with available parallelism; do not add low-priority wrappers or single-job limits to user-run commands.
- Static analysis and other checks that do not require compilation are permitted.
- Do not perform opportunistic refactoring, cleanup, formatting, or API changes merely because they are convenient while working nearby.
- If the task reveals additional work that is related but not required for the current development unit, identify it as discovered work and defer it to an appropriate phase boundary. Incorporate it into the active plan only when it falls within the authorized scope and work mode; otherwise report it for user direction.
- If newly discovered work is required for correctness or for the planned work to proceed, revise the plan within the authorized scope. If it requires an unauthorized work mode, stop at that boundary and request direction unless an approved plan already covers the transition.

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

Before beginning non-trivial implementation work, establish a concise, mode-scoped implementation plan containing coherent tasks and subtasks. Keep the plan in the working context unless a persistent planning artifact is explicitly requested.

Order the work deliberately so foundational decisions, architectural constraints, risky assumptions, and dependency boundaries are resolved as early as practical. Prefer sequencing that reduces the likelihood of later refactoring, prevents avoidable bugs, and allows later tasks to build on stable completed work.

Identify boundaries where completing one task may expose additional required work or invalidate an assumption, and structure the plan so those discoveries occur before large amounts of dependent implementation accumulate.

Follow the established plan within the authorized work scope unless implementation reveals information that materially changes the design or invalidates part of the plan.

A plan does not independently authorize transitions into other work modes. When a new dependency or architectural decision requires work outside that scope, identify the boundary and request direction unless the transition was previously approved.

Complete and commit coherent development units independently so the commit history reflects the progression of the implementation and preserves completed work as later development proceeds.

## Testing requests

Testing-request documentation is maintained only when explicitly authorized. Do not update `docs/testing-requests.md` automatically during implementation, refactoring, verification, or other non-documentation phases.

Preserve discovered testing requirements in the task handoff until a testing-request maintenance phase is authorized. At that point, reconcile accumulated requirements against existing requests.

Separate test design, test implementation, and test execution according to the authorized work mode.

During authorized testing-request maintenance:

- Maintain `docs/testing-requests.md` as the user-facing queue of testing needed for implemented work. Reconcile accumulated testing needs with the current source, targets, configuration, reported results, and required coverage.
- Aggregate deferred testing across development cycles. Extend an existing request when it covers the same behavior and configuration; avoid duplicate case runs through owner and aggregate runners, and batch needed build targets into as few invocations as practical.
- Label each request as **Automated** or **QA**, with its platform, prerequisites, readiness and acceptance scope. Keep development progress in the owning plan rather than duplicating its macro-task checklist here.
- For each automated request, provide a runnable fenced command block. Include required CMake configuration, explicit build targets, environment opt-ins, working directory and test selection. State when a matching existing build can be reused.
- Command blocks must establish their working directory explicitly, including QA launch blocks. Commands launched from a document may inherit that document's directory; locate the checkout root rather than assuming the shell already runs there, and preserve the caller's starting directory.
- User-run command-line CMake configuration must explicitly select Ninja with `-G Ninja`. Build and test commands use that configured build directory.
- For each QA request, provide a brief detailed summary and concise requirement bullets covering setup, actions, expected observations and required platform/runtime modes.
- Mark requests blocked when fixtures, hardware or an observation harness are unavailable, and identify the next action. Do not invent commands or count skipped, unselected or device-free checks as the missing acceptance.
- When the user reports results, reconcile requests and owning plans during the next authorized maintenance phase against the actual tested revision, configuration, platforms and coverage. Remove accepted requests, preserve unresolved portions and repair affected links; retain durable coverage limits in the relevant subject guide rather than appending routine execution logs.

Submitting a request does not authorize agents to configure/build the project or execute tests. Existing repository authorization rules still apply.

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