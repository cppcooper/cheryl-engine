# Testing-request maintenance

This guide governs maintenance of [the user-facing testing queue](../testing-requests.md).
It does not contain the queue itself. Use the [workflow](workflow.md) for task
scope and progression, [testing and verification](testing.md) for operation
permissions and evidence, and [documentation](documentation.md) for general
documentation maintenance.

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

Submitting a testing request does not authorize operations that require explicit permission under [Verification and resource use](testing.md#verification-and-resource-use).
