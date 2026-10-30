# Agent instruction library

This directory holds focused instructions for working on Cheryl. The root
[AGENTS.md](../../AGENTS.md#instruction-index) is the canonical index: it routes
shared work, investigation, architecture, implementation, testing, Git and
documentation tasks. Start there and read the guides for the current objective.

The [shared workflow](workflow.md) applies to every task. Focused guides own their
specific methods and rules. Reading a guide does not authorize another work mode
or an operation requiring permission. Existing user authorization persists within
its scope; use it instead of asking again.

## Documentation and source owners

[Documentation workflow](documentation.md) owns writing, auditing, usefulness
review and maintenance policy. [API documentation](api-documentation.md) owns
source-comment coverage, compact class declarations and extraction/render checks.
[Documentation generation](documentation-building.md) owns the agent's preparation
and artifact-verification sequence; the linked published tooling guide owns exact
commands, cache behavior and hosting procedures.

The [documentation directory](../README.md) maps published and repository-only
material. [Published navigation](../site/index.md) owns the manual hierarchy;
subject guides and declaring source own engine contracts. Preserve the agreed
navigation and useful URLs/anchors while improving content and task handoffs.

These Markdown guides are loaded through the root index. Their directory does
not give them automatic per-directory instruction scope. `docs/AGENTS.md` routes
actual documentation work back to the appropriate guides, including documentation
of the instruction library itself.

## Maintain this library

- Keep essential entry safeguards and the task-to-guide index in root `AGENTS.md`.
  Put detailed rules and procedures in the focused owner. Summaries link that
  owner; they do not introduce competing policy.
- Before moving a rule, account for every requirement, exception, scope boundary
  and rationale that still applies. Update references to the old owner and check
  that a task entering through the root index reaches the new rule.
- Prefer extending an existing guide. Add a guide when it answers a distinct
  recurring task and reduces irrelevant context or repeated searches. Choose a
  clear trigger, required inputs, working method, boundaries and completion evidence.
- Record durable user decisions, such as the
  [fixes-develop consolidation strategy](git.md#consolidate-branches-into-fixes-develop),
  in their subject owner. Keep temporary progress and transcripts out of the library.
- Verify links, headings and policy preservation. Walk representative tasks through
  the index: investigation must not silently become implementation; a build must
  retain its operation boundary; a merge must reach the recorded history policy.
- Expand from observed needs and recheck the benefit on later tasks. Do not
  duplicate the human manual, actual tool configuration or a source inventory.
