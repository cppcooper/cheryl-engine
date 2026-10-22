# Cheryl Engine documentation

Choose the entry point for your work:

| Task | Start here |
| --- | --- |
| Look up a declaration | [C++ API](site/api/index.md) |
| Set up a game project | [Project setup](site/getting-started/project-setup.md) |
| Follow a complete example | [Tutorials](site/getting-started/create-a/index.md): First Game, actor/player/AI controllers, Character, and shared UIController/HUD |
| Find an operation | [Task index](site/getting-started/how-to/index.md) |
| Browse a subject | [Guides by topic](site/guides/index.md) |
| Understand system responsibilities and public types | [Engine Systems](site/engine-systems/index.md) |
| Change or extend Cheryl | [Contributing](site/development/contributing.md) and [Module authoring](site/development/modules.md) |
| Check capabilities or planned work | [Status inventory](STATUS.md) and [planning catalogue](planning/README.md) |

## Source layout

| Location | Owner and audience |
| --- | --- |
| `site/` | Canonical sources for the published manual and module references. |
| `site/generation/` | Generator, configuration, theme assets and tooling checks. Local HTML is written to `site/generation/generated/html/`. |
| `for-agents/` | Focused agent work instructions, indexed by root `AGENTS.md`: Git, investigation, design, implementation, testing and documentation. |
| `planning/`, `STATUS.md`, `testing-requests.md` | Repository-only plans, investigation follow-up, status and pending acceptance. |

The generator reads `site/`. Keep published prose there. Module `README.md`
files beside the code are relative symlinks to their canonical module pages;
edit and preview those pages through `site/modules/`. Repository-only pages use
ordinary relative links to the site sources. The site converts links to
repository-only documents into versioned source links.

## Game Development

[Project setup](site/getting-started/project-setup.md) provides the first working
source/SDK configuration. [Choose modules](site/modules/index.md) maps capabilities to
implementations; [Source and SDK integration](site/development/consuming-engine.md)
owns detailed linking, context assembly and deployment.

The [tutorial sequence](site/getting-started/create-a/index.md) starts with First Game,
then covers actor commands, Player/AI controllers, a Character, and a shared TGUI
UIController/HUD. The optional final discussion considers abstract interfaces. The
[maintained tutorial application](../projects/apps/tutorial-game/README.md)
implements the class-based lessons.

The [Task index](site/getting-started/how-to/index.md) links to instructions directly.
Browse the same procedures and references by subject through:

- [Assets and animation](site/assets/index.md)
- [Rendering and resources](site/rendering/index.md)
- [Input](site/input/index.md)
- [Text and fonts](site/text/index.md)
- [GUI](site/gui/index.md)
- [Audio](site/audio/index.md)
- [Runtime and concurrency](site/runtime/index.md), including ownership, cleanup and work delivery
- [Memory, state and utilities](site/utilities/index.md)
- [Logging and diagnostics](site/diagnostics/index.md)

[Engine Systems](site/engine-systems/index.md) completes the category with a map of
responsibilities, public types, ownership and relationships. Concrete module
references belong to the topic they implement. Their canonical prose lives in
`site/modules/`; the
[repository module catalogue](../projects/modules/README.md) maps their source owners.

## Engine Development

- [Contributing](site/development/contributing.md) and [C++ code style](site/development/code-style.md)
- [Build the engine and SDKs](site/development/building.md)
- [Module authoring](site/development/modules.md), [Integration contracts](site/architecture/interfaces-adapters.md) and [UI adapter authoring](site/development/ui-adapters.md)
- [Testing and validation](site/development/testing.md), [Component and SDK checks](site/development/architecture-validation.md), [Logging checks](site/development/logging-acceptance.md) and [Desktop and controller checks](site/development/native-desktop-checks.md)
- [Documentation tooling](site/development/documentation.md) and [Publishing](site/development/publishing-documentation.md)
- [CI and SDK artifacts](site/development/ci.md)

[Pending checks and QA](testing-requests.md) records outstanding acceptance.
The [planning catalogue](planning/README.md) links active work and its prerequisites.

## Documentation organization

[Agent instructions](../AGENTS.md#instruction-index) route work through the
focused [instruction library](for-agents/README.md), including page outlines,
writing and precision checks, API comments and documentation generation. These
instructions are repository-only.

[The published navigation](site/index.md) and each parent page's toctree own the page
hierarchy. Give a page one navigation parent; use ordinary links for other routes
to its instructions. Keep task groups in the Task index's local contents and
implementation-specific contracts in their owning references.

Published paths can remain stable when a page's navigation home changes. Preserve
useful old anchors when consolidating content. Use relative links to documents
and source files. Keep published pages in the relevant topic folder under `site/`, with
lowercase, hyphenated filenames and `README.md` reserved for repository indexes.
