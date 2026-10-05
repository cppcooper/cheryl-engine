# Cheryl Engine Agent Instructions

## Repository safety

- Do not push commits or branches to any remote unless explicitly requested.
- Do not build or compile the project unless explicitly requested.
- When building is permitted, make an attempt to avoid running consecutive builds.
- Do not run tests unless explicitly requested.
- When testing is permitted, make an attempt to avoid running consecutive tests.
- Instead of consecutive builds and tests, make efforts to achieve the same end result in as few steps as possible.
- If consecutive builds must be used, then reduce the process priority and or thread count involved to avoid melting the CPU.
- Static analysis and other checks that do not require compilation are permitted.
- Do not perform opportunistic refactoring, cleanup, formatting, or API changes merely because they are convenient while working nearby.
- If the task reveals additional work that is related but not required for the current development unit, record it as discovered work and incorporate it into the remaining task plan at an appropriate boundary.
- If newly discovered work is required for correctness or for the planned work to proceed, treat it as part of the task and revise the plan accordingly.

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

Before beginning non-trivial implementation work, create an implementation plan that breaks the work into coherent tasks and subtasks.

Order the work deliberately so foundational decisions, architectural constraints, risky assumptions, and dependency boundaries are resolved as early as practical. Prefer sequencing that reduces the likelihood of later refactoring, prevents avoidable bugs, and allows later tasks to build on stable completed work.

Identify boundaries where completing one task may expose additional required work or invalidate an assumption, and structure the plan so those discoveries occur before large amounts of dependent implementation accumulate.

Once the plan is established, follow it unless implementation reveals information that materially changes the design or makes part of the plan invalid.

Complete and commit coherent development units independently so the commit history reflects the progression of the implementation and preserves completed work as later development proceeds.

## Documentation discipline

Documentation should help a reader understand the current system or complete future work. Keep context proportional to the task: start with the relevant roadmap or subject document and follow related links as needed rather than loading the entire planning folder.

Apply these rules to affected documents across `docs/`:

- Describe current contracts in the present tense. Keep plans focused on the target state, unresolved work, prerequisites, ownership/dependency constraints, ordered steps, discovery boundaries and acceptance criteria.
- Keep one concise progress checklist for each active macro task in the plan that owns its details. Update subtask checkboxes as work finishes, retaining checked subtasks while the macro task remains open. Keep future macro tasks at roadmap detail until they become active; link to the owning checklist instead of maintaining duplicate progress lists.
- Remove completed macro-task narratives and superseded alternatives. Document resulting durable behavior in its authoritative subject document, and link to it instead of duplicating contracts or source inventories.
- Preserve unique requirements, decisions and rationale that still guide implementation. Before shortening or deleting a plan, verify that each remains available in the surviving text or a linked authoritative document. Do not replace relevant detail with a vague summary or a pointer to Git alone.
- Git commits and diffs hold implementation history. A brief decision/history summary with specific Git references is useful when it explains the current direction or saves a future reader substantial reconstruction; do not reproduce the task-by-task journal.
- Do not append routine successful build/test/check reports, pass counts, transcripts or toolchain snapshots unless explicitly requested. Retain reusable validation procedures and meaningful coverage limits. Distinguish source completion from executable acceptance; a successful exit with skipped or unselected checks does not establish the missing coverage.
- Record unresolved failures, unavailable prerequisites and environment/compatibility limits only to the extent they affect future work, with the required next action. Remove resolved blockers unless they describe an enduring constraint.
- After each coherent unit, reconcile affected plans and current-state docs. When a macro task completes, remove its task section and checklist after preserving any reusable procedure, durable contract or still-relevant decision rationale and repairing inbound links. Delete a one-purpose plan when no unresolved work remains. Do not move its execution log elsewhere merely to empty the plan.

Keep a paragraph when it explains how something works, how it should work, how to get there, or a decision/constraint needed to do so correctly. Concision must preserve that information.

## Git

For commits created by the agent, use:

- `user.name = cppcooper`
- `user.email = cppcooper@users.noreply.github.com`

Apply this identity per Git command using `git -c`. Do not modify the user's global Git configuration or the repository's local Git configuration.

Commit messages must follow the project's convention and begin with one of:

- `Adds`
- `Updates`
- `Revises`
- `Deletes`
- `Fixes`

Use detailed commit messages appropriate to the completed development unit.

Preserve the individual commit series. Do not squash, amend, or otherwise rewrite completed commits unless explicitly requested.

Do not generate mailbox patches unless explicitly requested.

## Cheryl conventions

- Follow the repository's existing style and `.clang-format`.
- When organizing class declarations, place data members above methods.
- Keep unit-test names short and intuitive rather than long and descriptive.
- Do not use "no callers" or "few callers" as grounds for removing an interface. Evaluate interfaces according to their architectural purpose and contract.
