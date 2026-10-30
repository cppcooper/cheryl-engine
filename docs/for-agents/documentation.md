# Write and review useful documentation

Help the intended reader finish a task or answer a question without guessing.
Start with a plain explanation, then provide the exact rules needed to use the
system correctly. A concise entry and detailed contracts can belong on the same
page when headings and links make each easy to find.

Use the [shared work rules](workflow.md) for authorization and phase boundaries.
This guide owns documentation audit, authoring and maintenance policy. The
[documentation directory](../README.md) identifies existing owners; the
[tooling guide](../site/development/documentation.md) owns generation commands,
outputs, caches and tool selection.

## Establish the task and its owner

Before editing, inspect Git status and relevant existing changes, then identify
the reader's task, the content being changed and its authoritative owner. Choose
the appropriate operation:

| Operation | Starting action | Required result |
| --- | --- | --- |
| Create | Search existing pages and public declarations for the answer. | Fill a distinct unanswered need in its existing owner, or justify a new page. |
| Improve | Read the whole affected unit, its entry route, relevant source and connected examples. | Repair a specific reader problem while preserving correct contracts and useful detail. |
| Audit | Define the corpus, questions and evidence before judging content. | Report supported findings and limits; follow the repository's audit/maintenance boundary. |
| Maintain | Trace the changed contract or completed work through its owners and dependents. | Reconcile current instructions, examples, links, plans and acceptance scope. |

Keep the working scope concise: audience, intended outcome, starting scaffold,
affected owners, evidence and completion checks. Record unresolved implementation
questions separately from prose defects. A documentation task must not silently
change an API or invent behavior to make an example easier.

### Place the answer

- Keep published prose under `docs/site/`. Edit module references there; neighboring
  module READMEs are aliases. Keep agent guidance and repository plans outside the
  published source root. See the [source layout](../README.md#source-layout).
- Update the existing owner before adding another explanation of the same rule.
  An example may summarize the rule at the point of use and link its full contract.
- Preserve the established navigation, category order and tutorial progression
  during content repair. Add useful local headings and contextual routes. A new
  page needs a distinct reader task and one navigation parent in the existing
  hierarchy; secondary routes use ordinary links.
- Match the page's title, introduction and incoming labels to the question it
  answers. Preserve useful paths and section anchors when consolidating content.
- Source comments own declaration-specific API contracts; subject guides own
  cross-system explanations and procedures. Follow the
  [API instructions](api-documentation.md) for generated reference work.

### Establish the evidence

Read the responsible declaration, implementation and relevant tests/configuration
before writing a current contract. Follow a convenience operation far enough to
verify the dependencies on which its promise relies. Tests provide evidence of
specific cases; source and test presence alone do not establish executable acceptance.

Prefer the supported engine path and defaults when teaching application setup.
If an example requires application-owned plumbing, confirm that no existing engine
service supplies it and explain the actual application responsibility. A missing
integration is discovered engineering work; do not spread an unexplained workaround
across tutorials as though it were the intended architecture.

## Layer first use and precise reference

Start with the operation's purpose and normal supported path. Introduce a concept
when the reader first needs it. Follow with meaningful choices/customization and
the exact contracts, limits and failure rules. Give experts headings that lead
directly to those rules without rereading a lesson.

Keep ownership, lifetime, thread and failure constraints beside the step that can
violate them. Additional depth can live in the linked reference. A plain opening
must not hide a prerequisite, and a precise reference must not require a beginner
to infer the common sequence from a list of types.

## Choose the page's job

Before drafting, answer:

- Who is reading, and what should they accomplish or understand?
- What knowledge, files, configuration or working example do they start with?
- Which existing page owns the answer, and which source establishes the behavior?
- What result would show success, and where should the reader go next?

Use the role that matches that job. The outlines below are prompts; keep the
parts relevant to the page rather than adding empty sections.

| Role | Useful outline |
| --- | --- |
| Index | Purpose and reader choice → meaningful destination table or list → deeper routes. Explain differences between nearby choices. |
| Tutorial | Goal and expected result → prerequisites and starting scaffold → staged changes with concepts introduced at use → observe success → cleanup and next lesson. |
| How-to | Named starting point → exact changes and insertion points → expected result → relevant failures and limits → full contract. |
| System overview | Responsibility and type choices → relationships, ownership and execution → small integration route → detailed owners. |
| Conceptual explanation | Reader question → mental model and necessary terms → relationships and consequences → relevant choices → concrete task/reference. |
| Reference | Valid inputs, outputs and units → applicable ownership, lifetime, thread and state rules → failures and limits → examples and authoritative links. |
| Troubleshooting | Observable symptom → evidence that distinguishes likely causes → corrective action → expected observation and unresolved limits. |
| Plan or acceptance procedure | Current readiness → prerequisites and decisions → next action → observable acceptance. Follow [maintenance policy](#maintenance-policy). |
| API comment | Purpose → meaningful parameter/return semantics → relevant preconditions, ownership, execution and failures → owning guide or related declaration. |

An index succeeds through its choices and destinations. A simple alias or constant
may answer its lookup question through an accurate declaration and one contextual
link. A resource or lifecycle reference usually needs substantially more contract
detail. Examples should solve an actual reader problem.

### Task or tutorial starting outline

Adapt this to the size of the operation. A tutorial may repeat the change-and-observe
steps as it introduces concepts; a focused task may need only one such step.

```markdown
# Perform the operation

State the result and when the reader needs it.
Name the working example or starting files and required setup.

## Make the change

Identify the file, class/member, hook or configuration to change.
Introduce necessary concepts before using them.
Show the change and explain why it produces the result.

## Observe the result

Describe an observable success condition and relevant failure behavior.
Include cleanup when the operation creates a lifetime to manage.

## Continue

Link the next task and the authoritative detailed contract.
```

An intermediate lesson still needs a checkpoint: a defined return value, state
change or other inspectable result. Say which later lesson makes it visible in
the application.

### Reference starting outline

```markdown
# Name the interface or contract

Explain its purpose and when to choose it. Link the first-use sequence.

## Inputs and results

Define valid values, units, defaults and returned ownership.

## Ownership and execution

State relevant lifetimes, allowed callers/threads and publication boundaries.

## State, failures and limits

Describe transitions, rejection, preserved state and unsupported cases.

## Examples and sources

Give a focused example where it helps; link deeper usage and declaring sources.
```

For generated reference pages, use the [API instructions](api-documentation.md)
for comment placement, complete entity coverage and source-to-render checks.

## Start plainly, then give the exact contract

- Put the reader's answer in the opening paragraph. State what the operation
  accomplishes before describing its internal machinery.
- Define necessary terms at first use. Keep names consistent across prose,
  snippets, assets and linked examples.
- Give each paragraph one main point. Prefer concrete subjects and verbs over
  vague claims such as "handles everything" or "works automatically."
- State units, defaults and important boundaries explicitly. For example, identify
  whether a width uses logical coordinates or framebuffer pixels.
- Give snippets their insertion context: file, member declarations, hook, required
  includes, configuration and asset prerequisites as needed. Identify placeholders
  and distinguish a complete example from a fragment.
- Make the simple path easy to follow, then expose exceptions and exact rules
  under descriptive headings. Keep a failure or lifetime rule beside the step
  that needs it, even when the full explanation belongs deeper.
- Describe the expected observation. "It builds" and "the requested behavior
  works" are different outcomes.
- Preserve useful expert detail. Improve its introduction, ordering or retrieval
  when readers struggle to find it; remove repetition after preserving its unique
  requirements in the authoritative owner.

Use short, concrete headings and task labels. Tables help readers choose between
paths or compare rules; use columns that support that decision. Avoid repeated
promises such as "safely" without stating the actual constraint. Use a diagram
when it clarifies a relationship, ownership boundary or sequence; accompany it with
the essential rule in text and confirm the generator can render its format.
Use descriptive link text and language-tagged code fences. Explain meaningful
images/diagrams in text and label units and relationships; color alone must not
carry a required distinction. Check wide tables and long signatures in the
rendered page instead of assuming they remain readable.

## Make examples and procedures complete

- Name the initial working scaffold, source/SDK selection and relevant options.
  Identify exact files, declarations, members, hooks, includes, link targets and
  asset prerequisites. Show where a fragment belongs and what existing code it
  replaces or retains. Do not present a fragment as a complete application.
- Follow the full relevant lifecycle: setup, use, observation and release. A lesson
  ending in prepared state must identify how that state is inspected and which
  lesson makes it visible.
- Prefer maintained example code or includes when they serve the lesson. Check
  the lesson's conceptual explanation against that code; inclusion alone does not
  establish that the reader understands the missing integration.
- Keep identifiers, font-group names, assets, actions, options and defaults
  consistent across linked lessons and references. Check an example's requested
  identity as well as its plausible output when fallback can hide a mismatch.
- Explain whether paths are relative to the checkout, asset root, executable or
  current directory. Command blocks establish their working directory and preserve
  the caller's starting directory. Follow existing repository command conventions.
- Separate ordinary success from expected rejection/failure examples. Explain
  what the reader observes, what remains valid and how to recover. Diagnostics
  should lead to an action rather than repeat internal implementation names.
- State supported configurations and necessary fixtures at the point of use.
  Validate selected platform, static/shared linkage, packaging and runtime claims
  along their affected producer-to-consumer path. Do not infer all configurations
  from one successful example.

## Support the claims

Add technical detail when it helps the reader choose, use or verify the system.
Check that precision against the responsible source. Unsupported specificity
creates an expectation the documentation cannot justify.

For each consequential statement, identify the source that establishes it:

| Claim | Evidence to inspect |
| --- | --- |
| Current behavior | Declaring interface and relevant implementation, including failure paths. |
| Selection, linking, installation or deployment | The configuration owner and the affected producer-to-consumer path. |
| Intended change | The owning design or plan, clearly distinguished from current behavior. |
| Accepted behavior | Results for the stated revision, configuration and coverage, with unresolved limits retained. |

Use precise scope: who owns a value, what a view borrows, when a change becomes
visible, which thread may perform an operation, and what failure leaves intact.
Follow a convenience API through its dependencies before promising behavior on
their behalf.

If evidence is incomplete, state what is known and what remains uncertain. Label
an inference and identify the missing check. A declaration's presence establishes
that it was extracted; inspect the rendered signature and meaning before claiming
accurate or complete API documentation.

Keep one authority per fact. Other pages can summarize enough to make a decision
and link the exact contract. Check linked examples together: a name mismatch can
silently select a valid fallback and produce a plausible result.

For punctuation in headings, distinguish the Markdown heading slug from the
final HTML ID. MyST can resolve `#shadermaterial-definitions` to HTML
`#shader-material-definitions`; copying the latter into a Markdown fragment can
produce an unresolved reference. Check the source form through Sphinx. API
`cheryl-guide:` links enter after that resolution and need the final HTML
fragment. Explicit labels preserve old HTML bookmarks, but a Markdown
`page.md#fragment` still needs that page's recognized heading slug; use the
current heading or a label reference.

## Review and audit

For a documentation audit, inspect the relevant implementation and documentation
for inaccurate, obsolete, missing, duplicated or unnecessary information. Report
findings and recommended changes without modifying documents. Distinguish intended
behavior from a mistaken claim about current implementation.

### Apply existing findings

The [usefulness report](../../reports/documentation-usefulness/report.md) and its
per-page inventories describe a recorded baseline. Recheck each finding against
the current source and content before using it as a task. Do not overwrite the
historical scores with assumed improvements or treat an old gap as proof that
the corresponding source/test is still absent.

Prioritize reader harm: false instructions and declaration errors; blocked task
handoffs and inconsistent examples; missing API contracts and guide routes; then
entry explanations, retrieval and scaffolds. Use the requested implementation
sequence when work is divided into stages. A high-scoring page can still contain
a consequential defect. Work order does not narrow a request to cover all content.

Repair a coherent subject or reader route, including its affected examples and
links, then review it before expanding the next batch. State the problem repaired
and its evidence; a higher score, longer comment or larger page is not the outcome.

### Bound a new audit

- Establish the requested audience and corpus before scoring. For a published-site
  audit, use the generator's source root, exclusions and include relationships to
  identify the rendered content. A repository link does not make its target a
  published page.
- Count included canonical content once. Keep manual pages and generated
  declaration articles in separate summaries, and put any repository-only review
  outside the primary results and priorities.
- Record the source revision and distinguish current observations from a previous
  report's findings. Use the existing navigation to find each page's job, entry
  context and deeper owners.
- Identify the expected improvement for the reader. A higher score should reflect
  a repaired instruction, supported contract or clearer route to an answer.
- Read every substantive unit claimed as reviewed, including relevant member
  documentation, rather than extrapolating from a sample. Distinguish complete
  reading, selected claim checks and automated measurements. Identify uncertainty
  and evidence limits for claims that were not checked.

### Measure usefulness

Assess the page for its role and intended reader:

| Dimension | Question |
| --- | --- |
| Purpose and place | Does the page answer the expected question from its stated prerequisites? |
| Beginner entry | Are the concepts and common path understandable to its intended first reader? |
| Practical usefulness | Can the reader finish the operation or answer the lookup question? |
| Contract depth | Can an expert verify the relevant inputs, ownership, execution, state and failures? |
| Currency and evidence | Do important claims match current source and distinguish implementation from acceptance? |
| Retrieval | Can readers scan to the next step or exact rule? |
| Connections | Can readers find the page, continue the task and reach its deeper owners? |

Use the report's [rubric and weights](../../reports/documentation-usefulness/report.md#rubric)
when numeric comparisons are requested. Preserve its role-sensitive interpretation
and cap for materially inaccurate content. Keep manual, generated API and
repository-only results separate. Comment counts, link counts and automated
readability measures are evidence screens; they do not establish semantic quality
or replace a reader walkthrough.

## Review the reader's path

Use the [usefulness rubric](../../reports/documentation-usefulness/report.md#rubric)
and [assessment method](../../reports/documentation-usefulness/report.md#2-how-the-assessment-was-made)
to structure judgment. The report's findings describe its recorded baseline;
check current source before carrying them into new work. Word counts, comment
presence and fine score differences do not establish usefulness.

Walk through two questions appropriate to the page:

1. **Starting reader:** Begin at the stated prerequisites. Follow the links and
   instructions to the result. Record missing setup, inferred steps, inconsistent
   names and unnecessary detours. Can the reader recognize success? If a fallback
   can hide a mistake, check the selected identity or state as well as appearance.
2. **Experienced reader:** Find an exact answer about a relevant input, owner,
   lifetime, thread, transition or failure. Can the reader cite the rule directly
   or reach its authoritative owner through a clear link?

Distinguish a static walkthrough from an executed example or observed user task.
Report what was inspected or run, the evidence it provides and remaining limits.
Use the existing verification rules to choose permitted checks. An unexecuted
example needs an honest status, not an implied successful result.

### Check discovery

Start from likely task words, the relevant overview and an API symbol reached
through search. Can each route reach the capability and its complete procedure?
Check whether the documentation answers what the engine supplies, what the
application chooses and which integration point to use. Follow the actual route;
a destination's existence is not evidence that a reader can find it.

For a gap, identify the reader's question, entry point, route attempted, missing
answer and smallest useful correction. Prefer a clear label, contextual link or
missing section in the existing owner over a duplicate page or navigation redesign.

## Maintain, consolidate and retire

After a contract changes, follow its impact through API comments, the owning guide,
summaries, examples, procedures and inbound links. Update the affected meanings
together; retain valid detail outside the change. Distinguish observed behavior
from intended changes when source and documented architecture disagree.

Before removing a document or section, identify its unique requirements, rationale,
procedures and unresolved acceptance. Preserve still-useful material in an existing
owner, then repair incoming links and useful anchors. Completed implementation
history belongs in Git; current contracts belong in references and unfinished work
belongs in its owning plan. Follow the [maintenance policy](#maintenance-policy) below.

Keep `STATUS.md` as the overview of capabilities and planned work. `planning/`
holds actual plans and an index of those plans. Reconcile both directions: each
actual plan needs a STATUS route; STATUS topics without a concrete plan remain
explicit scoping gaps. Separate ready acceptance in the testing queue from
unimplemented regressions and deferred fixture/device prerequisites.

### Maintenance policy

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

Historical audit data retains its reviewed revision and findings. Repair live
report links when owners move; use revision links for retired evidence. A later
audit records its new scope and evidence rather than silently rewriting history.

## Complete a documentation unit

Before handing off, establish the following for the affected scope:

- The intended reader can find the answer, follow the normal path and recognize
  success; experts can retrieve precise rules and their source.
- Consequential claims, names, examples and configuration boundaries agree with
  the responsible source and connected documents.
- The change preserves useful contracts, established navigation and relevant
  unresolved work; it introduces no competing authority or unexplained workaround.
- Applicable links, targets and fragments resolve. Inspect the rendered result
  for presentation-sensitive changes and API work; source Markdown or XML alone
  does not prove usable HTML.
- When generation is part of the authorized task, use the regular output at
  `docs/site/generation/generated/html/`, following the tooling guide. Record the
  actual output and revision; do not substitute a build-directory copy.
- Report changed content, checks actually performed and outstanding evidence or
  decisions. Generation/link success does not prove that prose is accurate or
  that the documented application procedure ran successfully.
