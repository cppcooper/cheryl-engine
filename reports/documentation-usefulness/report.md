# Documentation usefulness review

**Reviewed:** 2026-10-18

**Source and generated-site baseline:** `44ed9cc85852c0779397e4820ed7597f0338a781`

**Primary scope:** meaningful content included in the generated documentation.
Repository-only reviews are retained in a separate supplementary appendix.

The manual and generator have since moved under `docs/site/`. Links in this
report follow their current locations; score inventories retain the paths at the
recorded baseline, when module references still used README includes.
Retired repository-only documents in the supplementary table link to that baseline
so their historical ratings remain inspectable.

## Findings

The authored documentation is strongest at explaining precise contracts and placing
them in a coherent system. Its largest remaining weaknesses are stale instructions,
small missing steps in otherwise complete learning paths, and generated API pages
that omit contract explanations or occasionally misrepresent declarations.

- **91 published manual pages** were read in full and scored individually.
  Eight module pages include their canonical README content; each rendered page
  counts once in the results.
- **76 manual pages score at least 85/100**, ten score 70–84, and five need factual
  repair. The published-manual median is **93/100**. These are review judgments,
  not user success rates or a percentage of documentation completion.
- **607 generated declaration pages and 15 header/migration pages** were read and
  scored with the same role-aware usefulness rubric. The declaration median is
  **65/100**; 195 declaration pages score below 55. The remaining **368 mechanical
  indexes** are inventoried and explicitly excluded.
- The primary assessment covers **713 published content units**: 91 manual pages,
  607 declarations and 15 header/migration entries. These groups have separate
  summaries because their jobs and content sizes differ. Each has an individual
  recommendation. Sixteen repository-only documents are supplementary and do not
  affect these results or priorities.
- **447 declaration pages contain no formal explanatory prose** in their extracted
  declaration or public/protected member descriptions. Some have useful ordinary
  comments in their headers that never reach the rendered reference.
- **No declaration article directly links to a manual guide.** This measurement
  excludes the global sidebar, breadcrumbs and previous/next controls. Those provide
  navigation, but a reader arriving at a symbol from search still lacks a nearby
  route to its worked example or owning contract guide.

The highest return comes from correcting factual drift and declaration errors,
completing the small learning handoffs, and exposing existing public contract
explanations in the API reference.
The current hierarchy provides suitable homes for this work.

### Score files

| Artifact | Contents |
| --- | --- |
| [Published manual scores](published-scores.csv) | The 91 manual pages included in the generated site, with rendered paths, canonical content owners, seven ratings, strengths, gaps, recommendations and source evidence. |
| [API scores](api-scores.csv) | All 990 generated pages: 622 full usefulness reviews and 368 explicit exclusions, with ratings, recommendations, source evidence and separate declaration-coverage measurements. |
| [Complete review data](audit-data.json) | All reviews and evidence. `published_summary` owns the primary results; `repository_only_summary` and the full-inventory `summary` retain supplementary results explicitly. |
| [Per-page appendix](#per-page-scores) | Published manual scores, the 15 header/migration entries and 12 illustrative declaration reviews, followed by a separate repository-only appendix. All 607 declaration ratings are in the API CSV. |

## 1. What usefulness means

A useful page lets its intended reader make progress and verify the important
expectations without guessing. The appropriate outcome depends on its role:

| Role | A useful page provides |
| --- | --- |
| Entry or index | A clear choice of route, enough distinction to choose correctly, and a reliable next destination. |
| Tutorial | Prerequisites, concepts introduced before use, a complete sequence, an observable result and a clear continuation. |
| Task procedure | The starting scaffold, exact changes or operations, expected result, relevant failure handling and a route to the full contract. |
| System overview | Plain purpose, relationships, ownership and execution responsibilities, with direct links to implementation procedures and precise contracts. |
| Detailed reference | Exact valid inputs, units, ownership, lifetime, threading, state transitions, failures and limits where applicable. |
| Verification procedure | Current readiness, prerequisites, next decision/action, boundaries and an observable acceptance condition. |
| API declaration | An accurate signature and enough meaning to use or verify it: parameter/field semantics, important preconditions, lifetime and failure behavior, plus a route to the owning guide. |

Beginner support means an accessible entry for the intended audience. An expert
reference need not teach C++, and an index need not contain code. A tutorial should
not hide a necessary ownership rule merely to stay short. A reference should not
repeat an entire tutorial when a brief explanation and direct link suffice.

### Rubric

Each dimension receives an integer rating from 0 to 4.

| Code | Dimension | Weight | What was assessed |
| --- | --- | ---: | --- |
| P | Purpose and place | 10% | Can the reader tell what this page answers, who it serves and what must be known or prepared first? Does that match its location and incoming links? |
| B | Beginner entry and learning | 15% | Does the page introduce necessary concepts, distinguish easily confused terms, and provide a usable entry from its stated prerequisites? |
| X | Practical usefulness | 15% | Can the reader act or answer the intended question? Procedures need insertion/setup context and observable results; indexes need meaningful decision routes. |
| C | Contract depth | 20% | Can an experienced reader verify relevant ownership, execution, inputs, units, failures and unsupported cases, directly or through an appropriate authoritative handoff? |
| V | Verifiability and currency | 20% | Do current claims agree with source/configuration? Are historical, planned, implemented and accepted behavior distinguished? Can the reader trace an assertion? |
| R | Retrieval and scanning | 10% | Can readers find the relevant step or rule through local headings, tables and concise wording without losing necessary detail? |
| L | Layers and connections | 10% | Does the page connect the simple route to deeper detail, use consistent example names, and hand off to the next operation without unexplained integration work? |

Organization contributes directly through **P and L**, and through local retrieval
under **R**. This measures how content uses the established hierarchy, including
entry context, local ordering, example continuity and links to deeper answers.

| Rating | Meaning |
| ---: | --- |
| 0 | Missing, unusable or misleading for the page's role. |
| 1 | Weak; a material requirement or explanation is absent or wrong. |
| 2 | Adequate for a common case, with material gaps or required inference. |
| 3 | Strong, with limited gaps. |
| 4 | Complete for the assessed role and supported by the inspected evidence. |

`raw score = Σ(weight in percentage points × rating / 4)`.

For example, a rating of 3 in a dimension weighted 20% contributes 15 points.
A vector of seven 4s scores 100.

When **V is 0 or 1**, the final score is capped at **69**. A polished page with
materially wrong instructions must not appear in the strong band. Both raw and
final values remain in the data. Displayed scores round to whole numbers; bands
use the unrounded final value.

- **85–100:** strong for its role.
- **70–84:** useful, with gaps.
- **55–69:** needs repair.
- **Below 55:** weak.

A 100 means no material gap was identified within the reviewed role. It does not
mean the page is universally perfect, proven usable by beginners, or beyond useful
editing. Small score differences should not drive priorities.

## 2. How the assessment was made

### Full page review

The primary manual set is the 91 meaningful authored pages rendered in the
recorded generated site, including eight module references populated from their
canonical README files. Publication is determined by Sphinx's document inventory
and include relationships, checked against the existing HTML. A link to a file
on GitHub does not make that file part of the generated manual.

The initial inventory also reviewed 16 repository-only documents. These remain
in the supplementary appendix and complete data, excluded from the primary
scores, low-scoring examples and recommended work. In particular,
[`STATUS.md`](../../docs/STATUS.md), plans, the acceptance queue and the other repository-only
README files have no generated manual pages. The
[Sphinx exclusions](../../docs/site/generation/conf.py#L37) explicitly omit several of
these groups. Every reviewed unit retains its role, seven ratings, strength,
gap or absence of a material gap, recommendation and evidence.

Two short compatibility notices—[old Architecture entry](../../docs/site/architecture/index.md)
and [old assembly entry](../../docs/site/architecture/assembling-engine.md)—have no
independent substantive content to grade. Their purpose is preserving old routes.
Third-party/submodule documents, generated source listings, agent instructions and
non-page code/configuration files are outside the scored corpus. Relevant code
and configuration were inspected as evidence. Fourteen generated umbrella or
compatibility header pages contain authored purpose/linking prose; the generated
deprecation list contains migration advice. These 15 receive full content ratings.
The remaining 368 API file/directory/namespace and library lists contain no
independent explanatory content and are excluded, with a reason on every row.
Generated source listings remain supporting evidence, not separate content pages.
The audit's own artifacts are excluded from its baseline corpus.

### Reader walkthroughs

Routes were traced as a reader would use them, checking prerequisites, example
continuity, missing insertion steps, success checkpoints and the route to deeper
answers. These were static walkthroughs, not observed users or executed tutorials.

For example, setup provides a one-transition body link to First Game. Its rendered
**Next** footer instead leads through Choose modules, Source and SDK integration,
and Tutorials before First Game: four transitions. That is an optional conflicting
progression cue, not an unavoidable four-page prerequisite. An explicit lesson
continuation in the content would remove the ambiguity within the current layout.

### Source checks and expert questions

Central claims were compared with public headers, implementation, build policies,
maintained examples and workflows. Expert probes asked who owns a result, which
thread may use it, when a change becomes visible, what failure preserves and what
must remain alive. Source checks are recorded per authored page. They are selected
checks of important claims, not exhaustive proof of every sentence.

No builds, native tests, hosted workflow inspection, downloads or full documentation
generation were performed for this audit. Existing generated HTML at the same
baseline was inspected. Source evidence establishes documented implementation
behavior within that inspection; it does not add runtime or platform acceptance.

### Structural and API evidence measurements

Word, heading, code-block and link counts are included for diagnosis. They do
**not** determine authored scores. More prose, more examples and lower apparent
reading age are not automatically improvements. Formulaic readability scores were
avoided because C++ identifiers, command options and precise technical vocabulary
would distort them.

Each generated declaration also receives an **API evidence score**, separate from
its editorial usefulness score: 25 points each for a rendered declaration, a source
trace, explanatory prose, and a direct guide handoff in the article. A guide
handoff means a link to an authored manual page, excluding navigation controls.
The screen uses rendered HTML and matched Doxygen XML. It finds missing evidence;
it cannot judge whether prose is understandable or a contract is complete.

All 607 declaration pages have the first two features. The 160 with explanatory
prose score 75; the remaining 447 score 50. None has the fourth feature. A clear
constant may need little additional prose, while a resource provider needs much
more. Therefore these screen values are **not interchangeable with usefulness
ratings or a claim that every page below 75 needs a rewrite**.

All 607 declaration articles received the full editorial rubric after reading
complete rendered article bodies, without sidebar/footer text. Important source
contracts were checked for 266 declaration articles; 341 have medium-confidence
content/provenance reviews. For those, V is at most 3: traceability to a current
header does not prove every declaration detail or implied behavior is correct.
Twelve varied declarations received the initial deeper checks; they are retained
as illustrative examples, rather than used to estimate the other pages.
The 15 header/migration articles were reviewed by their entry or migration role.

This editorial pass distinguishes a transparent alias, which may need no extra
prose, from a field-only lifecycle or policy type, whose signature cannot explain
its required ownership, units or failure rules. It also distinguishes explanations
missing from the header from existing ordinary comments lost during extraction.

### Calibration and limits

Three parallel review passes scored the same changing-text, audio overview and
UTF-8 reference pages. Their ranges were **97.5–100**, **90–92.5**, and **81.25–87.5**.
The disagreements concerned density, entry context and the value of short
relationship tables. The main findings were consistent. These are assistant
reviews, not independent human usability results; the exercise does not establish
statistical reliability. Treat differences of roughly five points cautiously,
especially near a band boundary. A final consistency pass aligned the treatment
of include guards, private visibility helpers and equivalent constants across
API review batches.
Confidence in cited source contradictions is stronger than confidence in a fine
ordering of well-written pages. Confidence records the evidence available; it is
not another contribution to the usefulness score.

## 3. Results

### Published manual

| Band | Manual pages |
| --- | ---: |
| Strong, 85–100 | 76 |
| Useful with gaps, 70–84 | 10 |
| Needs repair, 55–69 | 5 |
| Weak, below 55 | 0 |

The mean is **91**, and the median is **93**. Each generated manual page counts
once, including module content supplied by an include. Repository-only content,
API pages and evidence screens are excluded from this manual average.
The lowest scores are [Worker execution](../../docs/site/runtime/worker-execution.md)
and [CI and SDK collection](../../docs/site/development/ci.md), both **68**.

| Dimension | Mean rating / 4 | Reading of the result |
| --- | ---: | --- |
| Purpose and place | 3.99 | The established organization gives pages clear jobs. |
| Beginner entry and learning | 3.24 | The most consistent opportunity is explaining or completing the entry path. |
| Practical usefulness | 3.53 | Most procedures work as instructions on inspection; a few omit the last step or setup context. |
| Contract depth | 3.89 | Precise ownership, concurrency and failure explanations are a major strength. |
| Verifiability and currency | 3.69 | Most checked claims match source, but five pages contain material drift or contradictions. |
| Retrieval and scanning | 3.37 | Some expert material is harder to retrieve than its technical quality warrants. |
| Layers and connections | 3.76 | Routes are generally good; a few example mismatches and missing handoffs interrupt them. |

### Generated reference content

| Band | Declaration articles | Header/migration articles |
| --- | ---: | ---: |
| Strong, 85–100 | 62 | 2 |
| Useful with gaps, 70–84 | 193 | 10 |
| Needs repair, 55–69 | 157 | 3 |
| Weak, below 55 | 195 | 0 |
| **Total** | **607** | **15** |

The declaration mean and median are **66** and **65**. The 15 header/migration
entries average **78**, with median **83**. These are editorial usefulness scores;
the separate 50/75 evidence screen above only measures the presence of four features.

| API dimension | Mean rating / 4 | Reading of the result |
| --- | ---: | --- |
| Purpose and place | 2.82 | Many symbols need a concise statement of their job. |
| Beginner entry and learning | 2.14 | Names and signatures frequently assume missing context. |
| Practical usefulness | 2.66 | Common fields and operations need the information needed to use them. |
| Contract depth | 2.11 | Ownership, units, valid states and failures are often unavailable in the article. |
| Verifiability and currency | 3.35 | Most declarations are traceable; selected semantic checks also found rendering errors. |
| Retrieval and scanning | 3.80 | Symbol lookup and short member lists generally scan easily. |
| Layers and connections | 1.63 | Nearby guide/example routes are the largest consistent omission. |

A low API score does not imply an implementation defect. It can mean the reader
cannot determine the intended behavior from the article, even when source and
manual guides elsewhere explain it well. Conversely, a compact standard alias
can score highly because its declaration completely answers its narrow question.
Eight include-guard/private-helper articles are assessed as publication noise:
filtering them is more useful than expanding their prose. Public visibility and
capability macros still need appropriate consumer-facing explanations.

### What the strong pages do well

| Example | Score | Why it is useful | Pattern to preserve |
| --- | ---: | --- | --- |
| [Display and update text](../../docs/site/getting-started/how-to/update-text.md) | 100 | Names the First Game scaffold, shows member/init/update/draw/cleanup changes, explains retained output and explicit retry, and states what should be visible. | A bounded operation, complete lifecycle and observable result, with deeper rules linked afterward. |
| [Animation task](../../docs/site/getting-started/how-to/animate-sprite.md) and [whole-image task](../../docs/site/getting-started/how-to/draw-sprite.md) | 100 each | Extend existing working code instead of inventing another unexplained application. | Reuse a named starting state and show the exact integration point. |
| [Font selection and layout](../../docs/site/assets/text-layout.md) | 98 | Connects configure-once fonts, saved random choices, fallback, shaping and retained rendering while preserving detailed limits. | Put the application operation and expert contract in connected layers. |
| [Pipelines and materials](../../docs/site/rendering/pipelines-and-materials.md) | 98 | Explains defaults and customization through complete composition rather than requiring the reader to design a material builder. | Explain what the engine supplies, what the application chooses and where an override enters. |
| [Event delivery](../../docs/site/runtime/event-delivery.md) | 98 | Gives working delivery examples and exact callback/error/lifetime boundaries. | Pair examples with rules that prevent plausible misuse. |
| [CPU resource lifetime](../../docs/site/resources/resource-lifetime.md) | 100 | Progresses from retained handles to precise slot/release behavior without losing the ownership story. | Introduce a simple model, then expose the exact invariant. |
| [Startup module](../../docs/site/modules/startup.md) | 100 | Keeps the standard bootstrap path concise and delegates the full application/deployment sequence to its owner. | A reference can be short when its handoffs are complete. |
| [Task index](../../docs/site/getting-started/how-to/index.md) | 100 | Lets a reader choose a concrete operation quickly. | Judge an index by its decisions and destinations, not its prose volume. |

The common strength is a chain of answers: **what to do → what it owns → what
should happen → what happens on failure → where to verify more detail**. Technical
depth helps when it answers one of those questions at the point it becomes relevant.

### What lowers usefulness

#### Factual drift

These findings are source or internal-document contradictions, not judgments that
the prose is too technical.

| Affected content | Score | Finding and evidence | Useful correction |
| --- | ---: | --- | --- |
| [Source and SDK integration](../../docs/site/development/consuming-engine.md), [Build engine and SDKs](../../docs/site/development/building.md) | 69 each | Linux deployment text leaves FreeType/HarfBuzz/ICU outside the SDK and describes development-package rediscovery. [Engine registration](../../projects/engine/CMakeLists.txt#L39), [SDK installation](../../cmake/CherylSdk.cmake#L147) and [package imports](../../cmake/templates/CherylConfig.cmake.in#L14) instead package/import non-system native dependencies. | Reconcile producer, package and consumer instructions from the same dependency registry. Preserve the distinction between packaged native libraries and actual platform/system requirements. |
| [Worker execution](../../docs/site/runtime/worker-execution.md) | 68 | Describes static hwloc ownership. [Workers CMake](../../projects/modules/runtime/workers/CMakeLists.txt#L43) registers a shared dependency; the current module README describes that policy correctly. | Correct the linkage paragraph and link one authoritative deployment policy. |
| [CI and SDK collection](../../docs/site/development/ci.md) | 68 | The guide denies release publication, conflicting with the [collection/release workflow](../../.github/workflows/ci.yml#L455) and [eligible release PR checks](../../.github/ci/release.py#L79). Collection now assembles four final packages and a plural manifest. | Describe collection and gated publication separately, with current artifacts and exact eligibility. Existence of this implementation does not establish a successful hosted release. |
| [Asset manifests](../../docs/site/assets/asset-manifests.md) | 69 | Tells First Game readers to keep shader files absent from its minimal root, and fails to describe the asset material default supplied by [sprite loading](../../projects/engine/src/core/resources/asset-management/sprite-mgr.cpp#L55). | Show the actual minimal asset tree and ordinary default path before optional shader/material overrides. |

There are smaller precision defects outside the capped group: the
[utilities overview](../../docs/site/engine-systems/utilities.md) assigns ownership of
the returned `Prepared` value to its loader; the
[diagnostics guide](../../docs/site/runtime/subsystem-diagnostics.md) says the warning
threshold is above 1024 while the source uses `>= 1024`. These should be corrected
without treating them as evidence that the surrounding architecture is wrong.

#### Missing steps between otherwise useful pages

| Reader journey | Result of the walkthrough | Improvement |
| --- | --- | --- |
| Copy a project and run it | Setup builds `my_game`, but the copied-project route lacks its own exact launch command and expected result. Other maintained-example launch commands do not close that loop. | Finish that route with working directory, executable, visible result, controls and normal exit. Make its First Game continuation obvious. |
| Controller → Character → HUD | The sequence is coherent. Controller has no command-value checkpoint; a Character insertion renames the pass used by retained text; the HUD's TGUI enablement needs the setup template's local-option rule. | Add those small integration/checkpoint statements at their point of use. Keep controller translation separate from character behavior. |
| Configure a font group, save a choice and use it in a toolkit | The font guide's persistence/fallback route is complete. Its configuration names the group `books`, but GUI/TGUI/RmlUi examples select `book-fonts`. [Missing groups fall back](../../projects/engine/src/text/font-service.cpp#L186), so the wrong result can look successful. | Use one group name across the linked sequence or show the prerequisite configuration explicitly. |
| Load an asset and animate it | Sprite/animation composition is coherent. The loading guide starts with the optional full artwork root and a MiniWorld key before making those package prerequisites clear. | Start with a controlled self-contained root, or state the artwork prerequisite before the first load. |
| Dispatch asynchronous work | The dispatch reference precisely states ownership and waiting rules, but a first user must reconstruct the submission/readiness/adoption sequence elsewhere. | Provide or directly link one complete small sequence; keep the precise thread rules beside it. |
| Explain why a released GPU handle remains resident | The expert answer exists across residency and diagnostics. A newcomer must first infer the meaning of a resource domain and the release/maintenance sequence. | Define the domain plainly and give a short release/maintenance/retirement checklist before the detailed states. |

Expert walkthroughs found exact answers for context detach/history, queued event
errors, frame ownership/shutdown and resource retirement. Their opportunity is
retrieval and an accessible entry, rather than additional technical volume.

#### Generated API accuracy, explanation and scope

Two kinds of rendering errors show why a declaration and source link do not
establish accuracy. The generated `is_chrono_duration<std::chrono::duration<...>>`
article lists both `std::false_type` and `std::true_type` as bases, whereas the
[actual specialization](../../projects/engine/include/cheryl/math/time.h#L17)
inherits only `std::true_type`. Its score is **28**. The corresponding `_v`
variable-template article omits the source's `constexpr` qualifier and scores
**61**. Checking all 33 standalone variable articles against their declaration
lines confirmed the same omission on **26 `constexpr` declarations**, including
mouse-control IDs, geometry counts and native texture tokens. Those articles'
ratings and evidence record the defect individually. These are observed generated
output defects; this audit does not establish which extraction/rendering stage
introduced them. Repair the generation path and cover rendered specialization/
qualifier semantics. Preserve the correct engine declarations.

The full content pass also shows why successful extraction is insufficient:

- `Updater` and `AbstractGame` preserve useful Doxygen operation contracts and score
  **91** and **89**. Their chief missing connection is a direct worked-guide link.
- `GameRuntime` scores **70**. Its overview is useful, but ordinary header comments
  about blocking `run()`, thread-safe `stop()` and restrictions on live diagnostics
  do not appear beside the rendered operations.
- `Resources::Provider` scores **68**. Its source explains optional capabilities,
  transient data copying and upload layout, but important method comments vanish
  from the article. A signature alone cannot reveal that a capability may reject.
- `FontStyle` and `ContextDefinition` score **51**. Their fields render, but their
  meaning and intended use remain largely implicit. The UTF-8 scalar function
  scores **50**, despite a strong encoding guide and useful ordinary header comments.
- `FontSource` scores **88**: a concise explanation can give a variant meaningful
  selection semantics without turning an alias page into a tutorial.
- `resolve_sprite` scores **95**: its rendered prose explains fallback composition
  and invalid/empty input behavior. Transparent aliases such as `Files::Path`,
  `Memory::Shared` and the standard duration aliases score **98**, demonstrating
  that extra prose is unnecessary when the declaration answers the bounded task.
- Field-only scene/recording and policy types often hide whether data is owned or
  borrowed, which coordinate units apply, or which state transitions are valid.
  Fix those missing meanings before adding general introductory prose.
- Header-entry pages usually identify the include's purpose, but conditional
  module/support headers can render without their conditional include choices.
  Explain the link-target/visibility rule and hand off to the module catalog.
  Include guards and `CE_DETAIL_*` helpers should leave the public article list;
  they can remain available in source listings.

Preserve the public contract comments in Doxygen-compatible form where they are
intended for users. Add specific guide handoffs through source documentation or
the shared API presentation. Change the owning comments/generation mechanism,
then regenerate; editing generated `.rst` files would not preserve the improvement.

## 4. Recommended order of work

| Order | Work | Primary dimensions | Completion evidence |
| --- | --- | --- | --- |
| 1 | Reconcile SDK deployment, worker linkage, CI/release artifacts and manifests. Correct generated specialization inheritance and missing declaration qualifiers. | V, C, X | Producer, consumer and workflow instructions agree with source. Rendered declarations preserve the inspected base types and `constexpr`; no old artifact tree or shader prerequisite remains on the common path. |
| 2 | Complete the copied-project run checkpoint and the small Controller/Character/HUD handoffs; align the font group examples. | B, X, L | A reader can follow each linked sequence with no missing field, option, identifier or final run step. The visible result and returned font reference confirm the intended feature/group rather than silent fallback. |
| 3 | Preserve public operation comments in generated API output and connect important symbols to their owning guides. Prioritize heavily used lifecycle/resource/input/text APIs using the per-page gaps. Filter include guards/private helpers from standalone publication. | C, L, B, P | Required thread, ownership, units, empty-state and failure expectations are visible on the symbol page or one explicit owning-guide link away. Public search results serve application questions. |
| 4 | Add compact entry examples/phase tables to dispatch, native failures, residency and validation. Align content headings with their navigation labels. | B, X, R | A first reader can find the common sequence before the specialized states; an expert can still jump directly to the exact rule. |
| 5 | Align the module-authoring scaffold with current linkage/export helpers and maintained owners. | V, X, L | The guide uses the selected static/shared library policy and export setup consistently with maintained module examples. |

Priority depends on likely reader harm and the importance of the path, not score
alone. A font-example mismatch in an otherwise strong page can deserve attention
before a lower-scoring deprecated-format reference. Likewise, repair a false
deployment rule before polishing a correct expert explanation.

The improvements should preserve the qualities already working: concise routing
pages, maintained example scaffolds, explicit lifecycle/failure rules and one owner
for each detailed contract. Add the missing explanation or connection where it
answers a reader's question; avoid repeating every setup and caveat on every page.

## 5. How to measure improvement afterward

The review can be repeated at a new recorded source revision using the same roles,
weights, evidence fields and journey questions. Recompute the arithmetic from
`audit-data.json`; reassess changed claims and connected examples. A score increase
alone is insufficient if a reader still cannot finish the task.

For actual usability evidence, use two groups of tasks:

| Readers and task | Measurements | Useful success condition |
| --- | --- | --- |
| New users: launch their copied game, add independent character controls, add/update the HUD, select and restore a specialty font. | Completion without assistance; missing/inferred steps; incorrect route visits; elapsed time; whether the displayed result and returned saved-font reference are the requested ones. | Complete each task from its stated prerequisites, with no engine source reading required to fill an instruction gap. Record where assistance was needed. |
| Experienced users: answer ownership, valid-thread, detach/retention, retry and SDK deployment questions. | Correct answers with cited evidence; pages/source files visited; unanswered questions; confident but incorrect answers. | Obtain the precise answer from the reference or an explicit nearby authoritative handoff. Include failure and unsupported cases in the questions. |

Record the reader's prior experience and setup conditions. Compare equivalent
tasks and environments before and after edits. Use median completion time only
after tasks actually complete; report failures separately. Do not infer usability
from word counts, link checks, build success or a faster expert who already knew
the answer. These user studies are proposed follow-up methods and were not run.

## Per-page scores

The compact vectors below are **P/B/X/C/V/R/L**, each out of 4. Full strengths,
gaps, recommendations and evidence are in [Published manual scores](published-scores.csv)
and the complete JSON. The eight
include wrappers inherit the score of their canonical module README. All 607
declaration scores are in [API scores](api-scores.csv); the twelve below illustrate
contrasting content patterns and are not a sampling estimate.

### Published manual

#### Learning and discovery

| Content unit | Score / 100 | P/B/X/C/V/R/L |
| --- | ---: | --- |
| [api/index.md](../../docs/site/api/index.md) | 94 | 4/3/4/4/4/3/4 |
| [assets/index.md](../../docs/site/assets/index.md) | 90 | 4/3/3/4/4/4/3 |
| [audio/index.md](../../docs/site/audio/index.md) | 100 | 4/4/4/4/4/4/4 |
| [development/consuming-engine.md](../../docs/site/development/consuming-engine.md) | 69 | 4/3/4/2/1/3/4 |
| [diagnostics/index.md](../../docs/site/diagnostics/index.md) | 96 | 4/3/4/4/4/4/4 |
| [getting-started/create-a/character.md](../../docs/site/getting-started/create-a/character.md) | 90 | 4/3/3/4/4/3/4 |
| [getting-started/create-a/controller.md](../../docs/site/getting-started/create-a/controller.md) | 93 | 4/3/3/4/4/4/4 |
| [getting-started/create-a/hud.md](../../docs/site/getting-started/create-a/hud.md) | 94 | 4/3/4/4/4/3/4 |
| [getting-started/create-a/index.md](../../docs/site/getting-started/create-a/index.md) | 100 | 4/4/4/4/4/4/4 |
| [getting-started/first-game.md](../../docs/site/getting-started/first-game.md) | 98 | 4/4/4/4/4/3/4 |
| [getting-started/how-to/animate-sprite.md](../../docs/site/getting-started/how-to/animate-sprite.md) | 100 | 4/4/4/4/4/4/4 |
| [getting-started/how-to/bind-input.md](../../docs/site/getting-started/how-to/bind-input.md) | 94 | 4/3/4/4/4/3/4 |
| [getting-started/how-to/create-gui.md](../../docs/site/getting-started/how-to/create-gui.md) | 94 | 4/3/4/4/4/3/4 |
| [getting-started/how-to/draw-sprite.md](../../docs/site/getting-started/how-to/draw-sprite.md) | 100 | 4/4/4/4/4/4/4 |
| [getting-started/how-to/index.md](../../docs/site/getting-started/how-to/index.md) | 100 | 4/4/4/4/4/4/4 |
| [getting-started/how-to/update-text.md](../../docs/site/getting-started/how-to/update-text.md) | 100 | 4/4/4/4/4/4/4 |
| [getting-started/how-to/use-camera.md](../../docs/site/getting-started/how-to/use-camera.md) | 93 | 4/3/3/4/4/4/4 |
| [getting-started/project-setup.md](../../docs/site/getting-started/project-setup.md) | 78 | 4/3/3/3/3/3/3 |
| [guides/index.md](../../docs/site/guides/index.md) | 100 | 4/4/4/4/4/4/4 |
| [index.md](../../docs/site/index.md) | 90 | 4/3/3/4/4/4/3 |
| [input/index.md](../../docs/site/input/index.md) | 100 | 4/4/4/4/4/4/4 |
| [modules/index.md](../../docs/site/modules/index.md) | 96 | 4/3/4/4/4/4/4 |
| [rendering/index.md](../../docs/site/rendering/index.md) | 94 | 4/3/4/4/4/4/3 |
| [runtime/index.md](../../docs/site/runtime/index.md) | 96 | 4/3/4/4/4/4/4 |
| [text/index.md](../../docs/site/text/index.md) | 100 | 4/4/4/4/4/4/4 |
| [utilities/index.md](../../docs/site/utilities/index.md) | 96 | 4/3/4/4/4/4/4 |

#### Systems and contracts

| Content unit | Score / 100 | P/B/X/C/V/R/L |
| --- | ---: | --- |
| [architecture/interfaces-adapters.md](../../docs/site/architecture/interfaces-adapters.md) | 90 | 4/3/3/4/4/3/4 |
| [architecture/ownership-cleanup.md](../../docs/site/architecture/ownership-cleanup.md) | 100 | 4/4/4/4/4/4/4 |
| [architecture/threads-data-flow.md](../../docs/site/architecture/threads-data-flow.md) | 100 | 4/4/4/4/4/4/4 |
| [engine-systems/assets.md](../../docs/site/engine-systems/assets.md) | 93 | 4/3/3/4/4/4/4 |
| [engine-systems/audio.md](../../docs/site/engine-systems/audio.md) | 93 | 4/3/3/4/4/4/4 |
| [engine-systems/diagnostics.md](../../docs/site/engine-systems/diagnostics.md) | 90 | 4/3/3/4/4/3/4 |
| [engine-systems/dispatch.md](../../docs/site/engine-systems/dispatch.md) | 90 | 4/3/3/4/4/3/4 |
| [engine-systems/display.md](../../docs/site/engine-systems/display.md) | 93 | 4/3/3/4/4/4/4 |
| [engine-systems/events.md](../../docs/site/engine-systems/events.md) | 100 | 4/4/4/4/4/4/4 |
| [engine-systems/gui.md](../../docs/site/engine-systems/gui.md) | 90 | 4/3/3/4/4/3/4 |
| [engine-systems/index.md](../../docs/site/engine-systems/index.md) | 100 | 4/4/4/4/4/4/4 |
| [engine-systems/input.md](../../docs/site/engine-systems/input.md) | 94 | 4/3/4/4/4/3/4 |
| [engine-systems/logging.md](../../docs/site/engine-systems/logging.md) | 90 | 4/3/3/4/4/3/4 |
| [engine-systems/memory.md](../../docs/site/engine-systems/memory.md) | 90 | 4/3/3/4/4/3/4 |
| [engine-systems/rendering.md](../../docs/site/engine-systems/rendering.md) | 94 | 4/3/4/4/4/3/4 |
| [engine-systems/resources.md](../../docs/site/engine-systems/resources.md) | 90 | 4/3/3/4/4/3/4 |
| [engine-systems/runtime.md](../../docs/site/engine-systems/runtime.md) | 90 | 4/3/3/4/4/3/4 |
| [engine-systems/state.md](../../docs/site/engine-systems/state.md) | 100 | 4/4/4/4/4/4/4 |
| [engine-systems/text.md](../../docs/site/engine-systems/text.md) | 94 | 4/3/4/4/4/3/4 |
| [engine-systems/utilities.md](../../docs/site/engine-systems/utilities.md) | 73 | 4/3/3/3/2/3/3 |
| [engine-systems/workers.md](../../docs/site/engine-systems/workers.md) | 90 | 4/3/3/4/4/3/4 |
| [resources/legacy-ffont.md](../../docs/site/resources/legacy-ffont.md) | 73 | 3/2/2/4/3/3/3 |
| [resources/resource-lifetime.md](../../docs/site/resources/resource-lifetime.md) | 100 | 4/4/4/4/4/4/4 |
| [resources/resource-residency.md](../../docs/site/resources/resource-residency.md) | 84 | 4/2/3/4/4/3/3 |
| [runtime/audio.md](../../docs/site/runtime/audio.md) | 90 | 4/3/3/4/4/4/3 |
| [runtime/display-and-window-contract.md](../../docs/site/runtime/display-and-window-contract.md) | 90 | 4/3/3/4/4/3/4 |
| [runtime/event-delivery.md](../../docs/site/runtime/event-delivery.md) | 98 | 4/4/4/4/4/3/4 |
| [runtime/failure-reporting.md](../../docs/site/runtime/failure-reporting.md) | 80 | 4/2/2/4/4/3/3 |
| [runtime/input-state-model.md](../../docs/site/runtime/input-state-model.md) | 98 | 4/4/4/4/4/3/4 |
| [runtime/logging.md](../../docs/site/runtime/logging.md) | 95 | 4/4/4/4/4/2/4 |
| [runtime/runtime-frame-boundary.md](../../docs/site/runtime/runtime-frame-boundary.md) | 90 | 4/3/3/4/4/3/4 |
| [runtime/simulation-timing.md](../../docs/site/runtime/simulation-timing.md) | 91 | 4/3/4/4/4/3/3 |
| [runtime/subsystem-diagnostics.md](../../docs/site/runtime/subsystem-diagnostics.md) | 93 | 4/4/4/4/3/3/4 |
| [runtime/thread-dispatch.md](../../docs/site/runtime/thread-dispatch.md) | 80 | 4/2/2/4/4/3/3 |
| [runtime/utility-contracts.md](../../docs/site/runtime/utility-contracts.md) | 83 | 4/3/3/4/3/3/3 |
| [runtime/worker-execution.md](../../docs/site/runtime/worker-execution.md) | 68 | 4/3/3/3/1/3/3 |

#### Assets, rendering and backend owners

| Content unit | Score / 100 | P/B/X/C/V/R/L |
| --- | ---: | --- |
| [assets/asset-loading.md](../../docs/site/assets/asset-loading.md) | 84 | 4/2/3/4/4/3/3 |
| [assets/asset-manifests.md](../../docs/site/assets/asset-manifests.md) | 69 | 4/3/3/3/1/3/4 |
| [assets/asset-values-and-playback.md](../../docs/site/assets/asset-values-and-playback.md) | 91 | 4/3/4/4/4/3/3 |
| [assets/catalog.md](../../docs/site/assets/catalog.md) | 80 | 4/3/3/3/3/4/3 |
| [assets/file-and-font-discovery.md](../../docs/site/assets/file-and-font-discovery.md) | 90 | 4/3/3/4/4/3/4 |
| [assets/text-encoding.md](../../docs/site/assets/text-encoding.md) | 86 | 4/2/3/4/4/4/3 |
| [assets/text-layout.md](../../docs/site/assets/text-layout.md) | 98 | 4/4/4/4/4/3/4 |
| [development/ui-adapters.md](../../docs/site/development/ui-adapters.md) | 85 | 4/3/3/4/4/2/3 |
| [gui/index.md](../../docs/site/gui/index.md) | 88 | 4/3/3/4/3/4/4 |
| [rendering/pipelines-and-materials.md](../../docs/site/rendering/pipelines-and-materials.md) | 98 | 4/4/4/4/4/3/4 |
| [projects/modules/audio/miniaudio/README.md](../../docs/site/modules/miniaudio.md) | 98 | 4/4/4/4/4/3/4 |
| [projects/modules/graphics/opengl/README.md](../../docs/site/modules/opengl.md) | 93 | 4/3/3/4/4/4/4 |
| [projects/modules/platform/debug-terminal-linux/README.md](../../docs/site/modules/debug-terminal-linux.md) | 98 | 4/4/4/4/4/3/4 |
| [projects/modules/platform/native-glfw/README.md](../../docs/site/modules/native-glfw.md) | 85 | 4/3/3/4/3/3/4 |
| [projects/modules/runtime/startup/README.md](../../docs/site/modules/startup.md) | 100 | 4/4/4/4/4/4/4 |
| [projects/modules/runtime/workers/README.md](../../docs/site/modules/workers.md) | 100 | 4/4/4/4/4/4/4 |
| [projects/modules/ui/rmlui/README.md](../../docs/site/modules/rmlui.md) | 89 | 4/3/4/4/3/3/4 |
| [projects/modules/ui/tgui/README.md](../../docs/site/modules/tgui.md) | 91 | 4/3/4/4/3/4/4 |

#### Engine development

| Content unit | Score / 100 | P/B/X/C/V/R/L |
| --- | ---: | --- |
| [development/architecture-validation.md](../../docs/site/development/architecture-validation.md) | 89 | 4/3/4/4/4/2/3 |
| [development/building.md](../../docs/site/development/building.md) | 69 | 4/3/4/4/1/3/4 |
| [development/ci.md](../../docs/site/development/ci.md) | 68 | 4/3/3/3/1/3/3 |
| [development/code-style.md](../../docs/site/development/code-style.md) | 98 | 4/4/4/4/4/4/3 |
| [development/contributing.md](../../docs/site/development/contributing.md) | 88 | 4/3/3/3/4/4/4 |
| [development/documentation.md](../../docs/site/development/documentation.md) | 98 | 4/4/4/4/4/3/4 |
| [development/logging-acceptance.md](../../docs/site/development/logging-acceptance.md) | 94 | 4/3/4/4/4/3/4 |
| [development/modules.md](../../docs/site/development/modules.md) | 80 | 4/3/3/3/3/3/4 |
| [development/native-desktop-checks.md](../../docs/site/development/native-desktop-checks.md) | 86 | 4/3/4/4/3/3/3 |
| [development/publishing-documentation.md](../../docs/site/development/publishing-documentation.md) | 94 | 4/3/4/4/4/3/4 |
| [development/testing.md](../../docs/site/development/testing.md) | 94 | 4/3/4/4/4/3/4 |

### Included module pages

These are shared content, not eight additional independent reviews.

| Published source | Canonical content | Inherited score |
| --- | --- | ---: |
| [docs/modules/miniaudio.md](../../docs/site/modules/miniaudio.md) | [projects/modules/audio/miniaudio/README.md](../../docs/site/modules/miniaudio.md) | 98 |
| [docs/modules/opengl.md](../../docs/site/modules/opengl.md) | [projects/modules/graphics/opengl/README.md](../../docs/site/modules/opengl.md) | 93 |
| [docs/modules/debug-terminal-linux.md](../../docs/site/modules/debug-terminal-linux.md) | [projects/modules/platform/debug-terminal-linux/README.md](../../docs/site/modules/debug-terminal-linux.md) | 98 |
| [docs/modules/native-glfw.md](../../docs/site/modules/native-glfw.md) | [projects/modules/platform/native-glfw/README.md](../../docs/site/modules/native-glfw.md) | 85 |
| [docs/modules/startup.md](../../docs/site/modules/startup.md) | [projects/modules/runtime/startup/README.md](../../docs/site/modules/startup.md) | 100 |
| [docs/modules/workers.md](../../docs/site/modules/workers.md) | [projects/modules/runtime/workers/README.md](../../docs/site/modules/workers.md) | 100 |
| [docs/modules/rmlui.md](../../docs/site/modules/rmlui.md) | [projects/modules/ui/rmlui/README.md](../../docs/site/modules/rmlui.md) | 89 |
| [docs/modules/tgui.md](../../docs/site/modules/tgui.md) | [projects/modules/ui/tgui/README.md](../../docs/site/modules/tgui.md) | 91 |

### Header and migration entries

These fifteen generated pages contain authored purpose or migration content beyond mechanical lists.

| Entry and source | Score / 100 | P/B/X/C/V/R/L |
| --- | ---: | --- |
| [File cheryl.h (engine/include/cheryl)](../../projects/engine/include/cheryl/cheryl.h) | 89 | 4/3/4/3/4/4/3 |
| [File engine.h (engine/include/cheryl)](../../projects/engine/include/cheryl/engine.h) | 80 | 4/3/3/3/4/3/2 |
| [File modules.h (engine/include/cheryl)](../../projects/engine/include/cheryl/modules.h) | 60 | 4/2/2/2/3/3/1 |
| [File support.h (engine/include/cheryl)](../../projects/engine/include/cheryl/support.h) | 60 | 4/2/2/2/3/3/1 |
| [File miniaudio.h (modules/audio/miniaudio/include/cheryl/modules)](../../projects/modules/audio/miniaudio/include/cheryl/modules/miniaudio.h) | 83 | 4/3/3/3/4/4/2 |
| [File opengl.h (modules/graphics/opengl/include/cheryl/modules)](../../projects/modules/graphics/opengl/include/cheryl/modules/opengl.h) | 83 | 4/3/3/3/4/4/2 |
| [File debug-terminal-linux.h (modules/platform/debug-terminal-linux/include/cheryl/modules)](../../projects/modules/platform/debug-terminal-linux/include/cheryl/modules/debug-terminal-linux.h) | 89 | 4/3/4/3/4/4/3 |
| [File debug-terminal.h (modules/platform/debug-terminal-linux/private/startup/include/cheryl/support)](../../projects/modules/platform/debug-terminal-linux/private/startup/include/cheryl/support/debug-terminal.h) | 58 | 3/2/2/2/3/3/1 |
| [File native-glfw.h (modules/platform/native-glfw/include/cheryl/modules)](../../projects/modules/platform/native-glfw/include/cheryl/modules/native-glfw.h) | 80 | 4/3/3/3/4/3/2 |
| [File startup.h (modules/runtime/startup/include/cheryl/modules)](../../projects/modules/runtime/startup/include/cheryl/modules/startup.h) | 83 | 4/3/3/3/4/4/2 |
| [File worker-topology-hwloc.h (modules/runtime/workers/include/cheryl/support)](../../projects/modules/runtime/workers/include/cheryl/support/worker-topology-hwloc.h) | 83 | 4/3/3/3/4/4/2 |
| [File worker-topology-json.h (modules/runtime/workers/include/cheryl/support)](../../projects/modules/runtime/workers/include/cheryl/support/worker-topology-json.h) | 83 | 4/3/3/3/4/4/2 |
| [File rmlui.h (modules/ui/rmlui/include/cheryl/modules)](../../projects/modules/ui/rmlui/include/cheryl/modules/rmlui.h) | 83 | 4/3/3/3/4/4/2 |
| [File tgui.h (modules/ui/tgui/include/cheryl/modules)](../../projects/modules/ui/tgui/include/cheryl/modules/tgui.h) | 83 | 4/3/3/3/4/4/2 |
| [Deprecated List (engine/include/cheryl/assets/types/2d)](../../projects/engine/include/cheryl/assets/types/2d/ffont.h) | 83 | 4/3/3/3/4/4/2 |

### Illustrative API declarations

These scores use the same usefulness rubric as the full API CSV. Links lead to the declaring headers; the CSV also records each rendered article path.

| Declaration and source | Usefulness / 100 | P/B/X/C/V/R/L | Main improvement |
| --- | ---: | --- | --- |
| [`CE::Text::FontService`](../../projects/engine/include/cheryl/text/font-service.h) | 83 | 4/3/3/3/4/4/2 | Add Doxygen summaries for the primary configuration/selection operations and a see-also link to configure-fonts/saved-choice guides; no tutorial duplication is needed. |
| [`CE::Text::Font`](../../projects/engine/include/cheryl/text/font.h) | 83 | 4/3/3/3/4/4/2 | Document empty-handle behavior and the retained-owner requirement at the accessors; add a link to the saved-reference/retained-style guide. |
| [`CE::Text::Rendering::Updater`](../../projects/engine/include/cheryl/text/text-updater.h) | 91 | 4/3/4/4/4/4/2 | Add a see-also route to Display and update text and the retained-update contract, keeping this concise operational reference. |
| [`CE::Input::Actions::ContextSubmission`](../../projects/engine/include/cheryl/core/controls/action-context.h) | 83 | 4/3/3/3/4/4/2 | Document empty/closed attach failure and link the runtime-consumer binding or Controller sequence. |
| [`CE::Game::AbstractGame`](../../projects/engine/include/cheryl/core/game-framework/abstract-game.h) | 89 | 4/3/4/4/4/3/2 | Keep hook contracts; split overview by execution/ownership and link First Game or lifecycle guide. |
| [`CE::Rendering::Resources::Provider`](../../projects/engine/include/cheryl/assets/resources/resource-provider.h) | 68 | 4/2/2/2/4/3/2 | Publish these existing method contracts as Doxygen comments and link the material/resource guide. Preserve the strong domain/ownership overview. |
| [`CE::Runtime::GameRuntime`](../../projects/engine/include/cheryl/core/game-framework/game-runtime.h) | 70 | 4/2/2/2/4/4/2 | Convert the existing public method contracts to Doxygen and link runtime lifecycle/timing procedures; no duplicate tutorial body is required. |
| [`CE::Text::FontStyle`](../../projects/engine/include/cheryl/text/font.h) | 51 | 1/1/2/1/4/4/1 | Add a concise type/field description, normal/bold weight examples and a see-also link to retained style selection. |
| [`CE::Input::Actions::ContextDefinition`](../../projects/engine/include/cheryl/core/controls/action-context.h) | 51 | 1/1/2/1/4/4/1 | Publish the existing definition ownership/local-namespace comments as Doxygen, describe empty collections, and link ContextSubmission plus the scoped-binding sequence. |
| [`CE::Text::decode_utf8_scalar(std::string_view, std::size_t)`](../../projects/engine/include/cheryl/text/utf8.h) | 50 | 2/1/1/1/4/4/1 | Publish the existing function comments as Doxygen with parameter/return semantics and link the encoding/source-index reference. |
| [`CE::Rendering::Sampling::Filter`](../../projects/engine/include/cheryl/assets/resources/sampler.h) | 66 | 2/2/3/2/4/4/1 | Add one sentence describing where this enum is used and link Options/pixel-art sampling; keep full sampler policy with its owner. |
| [`CE::Text::FontSource`](../../projects/engine/include/cheryl/text/font-collection.h) | 88 | 4/3/3/4/4/4/2 | Crosslink FontFile/FontCollection configuration and failure rules. |

The [complete API inventory](api-scores.csv) contains all 622 usefulness reviews and the 368 explicitly excluded mechanical indexes. Its evidence-score columns apply only to the 607 declaration articles.

## Supplementary repository-only reviews

These sixteen documents are outside the generated documentation. Their ratings
are retained for reference and excluded from the primary results, low-scoring
examples and recommended work. `STATUS.md` belongs here.

| Repository-only content | Score / 100 | P/B/X/C/V/R/L |
| --- | ---: | --- |
| [README.md](../../README.md) | 80 | 4/3/3/3/3/3/4 |
| [docs/README.md](../../docs/README.md) | 98 | 4/4/4/4/4/3/4 |
| [docs/STATUS.md](../../docs/STATUS.md) | 59 | 3/2/3/3/1/2/3 |
| [docs/external-work/rmlui-placeholder-issue.md](https://github.com/cppcooper/cheryl-engine/blob/44ed9cc85852c0779397e4820ed7597f0338a781/docs/external-work/rmlui-placeholder-issue.md) | 89 | 4/3/4/4/3/3/4 |
| [docs/planning/README.md](../../docs/planning/README.md) | 86 | 4/3/4/3/3/4/4 |
| [docs/planning/develop-review-and-development-plan.md](https://github.com/cppcooper/cheryl-engine/blob/44ed9cc85852c0779397e4820ed7597f0338a781/docs/planning/develop-review-and-development-plan.md) | 75 | 4/3/3/3/2/3/4 |
| [docs/planning/long-term/README.md](../../docs/planning/long-term/README.md) | 69 | 4/3/3/4/1/3/4 |
| [docs/planning/long-term/asset-manifest-metadata.md](../../docs/planning/long-term/asset-manifest-metadata.md) | 88 | 4/3/3/4/3/4/4 |
| [docs/planning/long-term/platform-acceptance.md](../../docs/planning/long-term/platform-acceptance.md) | 73 | 4/3/3/3/2/3/3 |
| [docs/planning/mid-term/README.md](../../docs/planning/mid-term/README.md) | 88 | 4/3/3/4/3/4/4 |
| [docs/planning/short-term/unicode-text.md](https://github.com/cppcooper/cheryl-engine/blob/44ed9cc85852c0779397e4820ed7597f0338a781/docs/planning/short-term/unicode-text.md) | 85 | 4/3/3/4/3/3/4 |
| [docs/testing-requests.md](../../docs/testing-requests.md) | 69 | 4/3/4/4/1/3/4 |
| [projects/apps/demo/README.md](../../projects/apps/demo/README.md) | 69 | 4/3/4/3/1/2/3 |
| [projects/apps/tutorial-game/README.md](../../projects/apps/tutorial-game/README.md) | 96 | 4/3/4/4/4/4/4 |
| [projects/modules/README.md](../../projects/modules/README.md) | 94 | 4/3/4/4/4/3/4 |
| [projects/modules/audio/miniaudio/tests/fixtures/README.md](../../projects/modules/audio/miniaudio/tests/fixtures/README.md) | 98 | 4/4/4/4/4/4/3 |

The [original full authored inventory](authored-scores.csv) retains all 115 source-file rows, including these supplementary reviews and the include wrappers. Its `published` column identifies participation in the site. Use [Published manual scores](published-scores.csv) for the primary 91-page comparison.
