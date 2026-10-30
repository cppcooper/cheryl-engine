# Cheryl Engine agent instructions

This file is the entry point for repository instructions. Read the
[shared work rules](docs/for-agents/workflow.md) for every task, then the focused
guides that apply. Linked instructions are part of this instruction set. Read
applicable nested `AGENTS.md` files as well; `docs/AGENTS.md` routes documentation
work, including API comments outside `docs/`.

## Essential safeguards

- Work within the user's authorized scope and progression. Existing authorization
  persists for relevant fixes and reruns; do not request it again unnecessarily.
- Inspect Git status and relevant diffs before edits. Protect unfinished user
  work, active runs, outputs, build configurations and caches. Do not extend
  unfinished work as a stable foundation without resolving that boundary.
- Follow the exact [operation permissions](docs/for-agents/testing.md#verification-and-resource-use)
  before builds, native tests, full documentation generation, downloads,
  installations, devices or external services. Available tools do not grant permission.
- Push only when explicitly requested. Follow the
  [Git guide](docs/for-agents/git.md) for identity, commits and branch integration.
- Keep independent work objectives separate. Related cleanup, general documentation
  changes and broader verification need their own scope; necessary implementation
  investigation and directly related test maintenance belong to the requested fix.

## Instruction index

| Work | Read |
| --- | --- |
| Every task: scope, source authority, progression and shared-tree safety | [Shared work rules](docs/for-agents/workflow.md) |
| Review a problem, investigate behavior or develop a plan | [Investigation and planning](docs/for-agents/investigation-and-planning.md) |
| Design architecture, interfaces or ownership boundaries | [Architecture and design](docs/for-agents/architecture.md) |
| Implement a design, fix behavior, refactor or change tooling/SDK/CI | [Implementation](docs/for-agents/implementation.md) |
| Design, implement or run tests; verify results; use Windows tooling | [Testing and verification](docs/for-agents/testing.md) |
| Maintain the pending testing/QA queue | [Testing-request maintenance](docs/for-agents/testing-requests.md) |
| Use Git, commit, branch or consolidate into `fixes-develop` | [Git](docs/for-agents/git.md), including the established squash strategy |
| Audit, create, improve, consolidate or retire documentation | [Documentation workflow](docs/for-agents/documentation.md) |
| Document API types, fields, methods and templates | [API documentation](docs/for-agents/api-documentation.md), with the documentation workflow |
| Build/check the local documentation or diagnose stale output | [Documentation generation](docs/for-agents/documentation-building.md) |
| Maintain or extend these instruction sets | [Agent instruction library](docs/for-agents/README.md) |

Choose guides by the work actually being done. A guide explains how to perform
its mode; it does not authorize starting an independent objective. Follow the
existing subject owner linked from that guide instead of loading the entire
library or planning directory.
