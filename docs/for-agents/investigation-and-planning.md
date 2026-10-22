# Investigate and plan work

Use this guide for review, investigation, research and working plans. The shared
[workflow](workflow.md) owns work-mode authorization, progression, source authority
and shared-tree safety. This guide supplies the method for establishing an answer
or a concrete next step within that scope.

## Frame the question

Identify the requested outcome and the decision the investigation must support.
State what is being reviewed, which contracts or owners may be affected, and what
evidence would answer the question. Read-only investigation does not authorize
implementation when a fix becomes apparent; retain that fix as a recommendation.

Acquire context purposefully. Start with the relevant declarations,
implementations, configuration and concrete consumers. Follow dependencies far
enough to establish the affected contract, rather than loading the whole project
or every plan by default. Consult the subject guide, roadmap or history when it
explains intended behavior or a still-relevant constraint.

When the user or another agent is changing the same source, establish what revision
the observations describe. Use a stable existing snapshot or commit-based reads
where appropriate; do not turn a transient mixed read into a claim about a coherent
implementation. Record relevant uncertainty and preserve ongoing work.

## Trace the contract and evidence

Find the owner and follow the path from caller to result, including applicable
ownership, lifetime, execution, failure and cleanup boundaries. Check whether an
existing API or supported composition already solves the apparent problem before
recommending another abstraction or application workaround.

Distinguish these findings in the working notes and handoff:

| Kind | State |
| --- | --- |
| Observed implementation | What the current source/configuration does, with a concrete path and location. |
| Documented intent | The requirement, design rationale or planned direction, identified as such. |
| Verified behavior | What an actually executed check establishes, with its configuration and limits. |
| Inference or uncertainty | The reasoning, missing evidence and consequence for the decision. |

Source inspection can establish a code path or declared contract; test presence
does not establish a successful run. If implementation and documented intent
disagree, explain both rather than silently treating either as correct. Expand
the investigation when a dependency invalidates the current assumption, and use
the shared workflow's boundaries when further work needs user direction.

## Form a working plan

When the investigation produces a plan, make the next work concrete without
accumulating implementation before its foundations are settled. Include:

- The intended result, scope and affected owner/contract.
- Coherent tasks in dependency order, with independent work identified where useful.
- Risky assumptions, unfinished-work dependencies and decisions that must be settled
  before accumulating dependent changes.
- Applicable completion and acceptance criteria, including the smallest relevant
  checks and any unavailable or unauthorized executable verification.
- Mandatory decision checkpoints from the user or approved plan.

Resolve foundational decisions before extending their dependency surface. Revise
the plan when evidence invalidates an assumption; explain material changes and
their effect on scope or acceptance. Routine implementation choices within the
agreed scope do not each require another approval. A plan itself does not authorize
an operation or an independent objective outside that scope.

Keep plans and phase handoffs in context unless a persistent artifact is explicitly
requested. For an authorized durable plan, follow the existing plan owner and the
[documentation-maintenance rules](documentation.md#maintenance-policy).
Do not create a new report or planning file merely to record intermediate progress.

For a design decision, continue with [architecture guidance](architecture.md).
For authorized code changes, the
[implementation preparation](implementation.md#prepare-a-coherent-development-unit)
owns the required plan and development-unit sequencing.
Read the [testing instructions](testing.md) before executing unfamiliar checks.

## Hand off the result

Give a concise answer supported by concrete source locations and relevant
consumers. Report findings and recommendations in review/investigation mode;
distinguish them from changes actually implemented. For a material problem, explain
its consequence, existing owner, smallest coherent correction and any decision
that requires user direction. Do not manufacture findings to fill a target count.

Carry forward established decisions, constraints, outstanding work and affected
contracts. State the evidence collected, verification limits and recommended next
action so the next phase can proceed without reconstructing the investigation.
Stop at the requested objective or mandatory decision boundary; do not begin an
independent implementation, audit or documentation phase on the strength of a
recommendation alone.
