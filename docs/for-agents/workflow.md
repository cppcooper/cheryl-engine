# Shared work rules

Read this guide for every task. The root [AGENTS.md](../../AGENTS.md) indexes
focused instructions; read the guides for the actual work being performed.
These instructions apply across the repository, including source comments outside
`docs/`. User authorization and constraints persist across relevant fixes and reruns.

## Work modes and progression

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

A request to implement or fix behavior includes necessary investigation, updates to affected tests, focused regression coverage where useful, and verification permitted by the [testing guide](testing.md). These supporting activities belong to the same development unit. Independent test design, broader verification campaigns, and documentation work remain separate objectives unless authorized.

User-directed progression is the default: complete the requested objective and stop before starting an independent objective. When a bounded sequence would benefit from plan-directed progression, propose its phases and decision checkpoints before adopting it. An approved plan may authorize progression across modes without further permission for each coherent subtask.

Stop when:

- The requested scope is complete or an approved plan reaches a mandatory decision checkpoint.
- A material requirement, architectural decision, or invalidated assumption requires user direction.
- A necessary operation or scope expansion is not already authorized.
- Continuing would depend on unfinished user work or risk modifying protected changes.
- A blocking condition prevents safe or correct completion.

A completed subtask or necessary supporting activity is not itself a stopping point. Continue authorized work that can safely proceed, including independent work unaffected by a blocked portion. Report any boundary, its implications, and the recommended next action.

At handoff, distinguish implemented changes, observed verification, and outstanding or deferred work. Completing an implementation phase does not establish executable acceptance or complete a broader task with required work remaining. Optional improvements and advisory documentation checkpoints neither block completion nor authorize further work.

## Source authority and context

Prefer source code, configuration, and actual implementation when determining the repository's current state and behavior.

Use documentation to understand intended architecture, requirements, design rationale, historical decisions, and planned work. Documentation may describe previous, intended, or partially implemented behavior.

Verify documentation's claims about current implementation against the relevant code. When implementation and documented intent disagree, distinguish the observed state from the intended state rather than automatically treating either as correct.

Acquire context purposefully. Inspect the code, documentation, history, and dependencies relevant to the current objective. Expand investigation when necessary rather than loading unrelated project context by default.

Prefer concise phase handoffs containing established decisions, relevant constraints, outstanding work, and affected contracts. Do not create persistent handoff or planning documents unless explicitly requested.

## Repository safety

- Preserve active user runs and their outputs. Do not change their configuration, clean their build directories, or invalidate their caches as a routine preparation step.
- Do not perform opportunistic refactoring, cleanup, formatting, or API changes merely because they are convenient while working nearby.
- Report related work outside the current development unit as discovered work. Resolve necessary dependencies within the authorized scope; use the progression boundaries above when further work requires user direction.

Git mutations and consolidation follow [Git](git.md). Builds, native execution,
full documentation generation, dependencies and devices follow the precise
permissions and resource rules in [Testing and verification](testing.md).
A mode guide or available tool does not grant authorization for its operations.

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

The [Git guide](git.md#create-coherent-commits) owns separating task commits from user work.

## Documentation phases

Reading documentation is permitted in any work mode when relevant to understanding requirements, intent, constraints, or history. Prefer the relevant source code when establishing current behavior, and consult the relevant roadmap or subject document for direction rather than loading the entire planning folder.

Documentation auditing and maintenance are separate work modes. Do not create, update, reorganize, or delete documentation during other work modes unless explicitly requested.

Essential code comments and runtime logging directly required by the implementation are permitted. Do not perform general comment cleanup, logging improvements, or explanatory documentation work opportunistically.

Defer documentation synchronization until a dedicated maintenance phase. Prefer stabilizing related implementation work before reconciling documentation.

Recommend a documentation checkpoint at a meaningful phase boundary when architectural contracts stabilize, substantial implementation changes accumulate, or documentation is likely to have become materially inaccurate.

Documentation checkpoints are advisory. Do not conduct a documentation audit or begin maintenance merely to make the recommendation. The user decides whether to continue engineering work or authorize documentation maintenance unless the transition was already approved.

For authorized auditing and maintenance, use the
[documentation workflow](documentation.md), plus the
[API instructions](api-documentation.md) when declarations or comments are involved.
A documentation audit reports findings without changing documents. Maintenance
requires its own authorization or an already approved sequence.

## Complete the requested scope

Use the relevant mode guide's completion criteria. Report implemented changes,
observed verification and remaining decisions or acceptance separately. Finish
independent authorized work when another portion reaches a boundary; do not turn
an optional improvement into either a blocker or authorization for another task.
