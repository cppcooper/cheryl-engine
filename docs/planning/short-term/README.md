# Short-term follow-up

This page owns unfinished regressions and dependency acceptance for implemented
font, Startup and UI behavior. The accepted Linux baseline remains in place;
these are separate gaps rather than a request to repeat all native QA. Subject
guides own current contracts, and the [testing queue](../../testing-requests.md)
owns ready acceptance requests. Each task needs its own authorized testing or
investigation phase before execution.

## Font-selection regressions

The [font-selection contract](../../site/assets/text-layout.md#font-selection)
and [implementation](../../../projects/engine/src/text/font-collection.cpp)
already rank discovered faces by declared weight/style. Existing discovery tests
cover lookup and ordering; registered-family CPU and GUI fixtures cover style
selection and collection face indices. The remaining task is automatic discovery
at numeric-weight boundaries.

- [x] Implement weight/style ranking and accept the existing selection, layout
  and resource checks; cover registered-family styles and face indices.
- [ ] Supply licensed fixtures with known style flags and numeric weights around
  the automatic limit of 500. Verify that a non-bold face at the limit is eligible,
  a face above it is excluded, and a bold flag also excludes an automatic face.
- [ ] Cover ranking around Regular (400), skipping a heavy-only automatic family,
  and an asset override whose heavy face suppresses that automatic family even
  when a lighter system copy exists.
- [ ] Verify that explicit preferred names, installed families and files remain
  unrestricted by the automatic-weight limit. Isolate asset/system directories
  from host-installed fonts and assert the selected face identity and metadata,
  so an embedded fallback cannot make a failed selection look successful.

Start with the existing
[selection fixtures](../../../projects/engine/tests/unit/src/font-selection.cpp)
and [font fixture helpers](../../../projects/engine/tests/include/testing/font-fixture.h).
Preserve fixture licensing and distinguish these cases from already covered
registered-family selection. Color emoji, wider CJK acceptance and editing/IME
remain outside the accepted [grayscale scope](../../site/assets/text-layout.md#native-acceptance-scope).

## Startup and UI follow-up

The [application guide](../../site/development/consuming-engine.md#command-line-startup)
owns parser, factory, result/context lifetime and option contracts. The
[adapter guide](../../site/development/ui-adapters.md#routing-and-unavailable-services)
owns complete-batch routing and focus epochs. The existing Linux root composition
has accepted native startup, field focus, placeholder and list-layout observations;
the [demo procedure](../../../projects/apps/demo/README.md#startup-and-ui-validation)
retains repeatable steps and coverage limits.

The [Startup suite](../../../projects/modules/runtime/startup/tests/src/startup.cpp)
already covers option registration, a supplied factory's invocation and worker
count, the backend validation callback, single-use initialization, help before
creation, invalid worker count and malformed argument arrays. With OpenGL startup
configured it also covers default option help, borrowed input remaining
uninitialized on help, invalid backend selection and a zero default window width;
without that backend it covers the standard factory's rejection. The
[CI runner](../../../.github/ci/run.py) selects `tests-startup`.

- [x] Implement common/backend Startup, complete-batch entry epochs and
  `before_record` controls; add the owned RmlUi correction and supplied-Core gate.
  Accept existing session/coexistence checks and independent installed-SDK Startup
  first-include probes.
- [x] Add the Startup parser, supplied-factory invocation and preconstruction
  validation regressions described above.
- [ ] Complete Startup factory/result handoff regressions: return a successful
  context with the selected runtime policy, reject a null factory result, and
  construct a runtime borrowing the successful result. The existing supplied
  factory probe deliberately throws before returning a context; the standard
  factory tests stop at help or invalid configuration. They do not exercise these
  result branches or successful native context construction. Keep native
  construction observations with the existing demo procedure.
- [ ] Add dedicated callback/batch-routing regressions. Deliver one complete
  batch while application controls run before individual records; verify that
  focus/visibility changes preserve the entry epoch and pending records, and
  that each callback's result selects pointer delivery for that record. Existing
  session/coexistence cases cover poll-latched preemption between calls; they do
  not call the `before_record` overload or change focus within its batch.
- [ ] Add a placeholder hit-testing regression exercising the corrected owned
  Core through public input and completed layout. Cover the state/selection cases
  in the [upstream follow-up](#upstream-placeholder-follow-up). Existing editing
  tests and accepted demo observations do not provide that dedicated regression.
- [ ] Complete the supplied dependency acceptance below with an independent host.

### Supplied RmlUi acceptance

Obtain a corrected RmlUi 6.3 target/package and a consuming host that bypass
Cheryl's source helper. Follow the full
[dependency contract](../../site/modules/rmlui.md#dependency-configuration), including
Engine font services and rasterizer ABI compatibility as well as the
[placeholder correction declaration](../../site/modules/rmlui.md#placeholder-dependency-contract).
Metadata must describe the actual library; setting a property does not correct it.

Accept the selected supplied-target or installed-package composition, resolving
aliases before declaring properties. Check rejection when the required correction
declaration is absent or false, independently of successful Cheryl-owned source
configuration. Exercise the supplied library's placeholder focus, editing and
shutdown in the consuming host. Record which supplied composition was exercised;
configuration rejection and downstream-owned native success do not establish
supplied-binary behavior.

## Upstream placeholder follow-up

The [RmlUi guide](../../site/modules/rmlui.md#placeholder-dependency-contract)
owns the source invariant and Cheryl's downstream correction. The affected pin is
RmlUi 6.3, `ba95ffe8bfb6370efb2cdcca927eaad4710c5413`. Source inspection establishes
a placeholder/display-length mismatch; it does not identify the instruction that
caused the original reported crash. No unmodified-upstream reproduction,
pre-fix sanitizer evidence or upstream submission has been completed for this task.

- [ ] Reproduce in a separate, unmodified upstream checkout, preserving shared
  `extern/rmlui`. Start at the pin, then check current upstream and record each full
  revision and actual runtime configuration. A guarded Cheryl build cannot serve
  as the pre-fix case. Recheck existing issues before proposing a submission.
- [ ] Load a real font, show the document, complete `Context::Update()` and verify
  a valid face, positive field width and formatted lines. Send mouse movement
  before button down/up through the public context API. Separate ordinary clicks
  in manual time unless deliberately testing double-clicks.
- [ ] Capture a demonstrably failing pre-fix regression, with a stack/memory
  diagnostic or another observable failure, then show the same case passing after
  the C++ correction. A value-stays-empty assertion alone can miss the invalid
  read. Include a long ASCII hint (for example 256 characters), verify its actual
  formatted length and click toward the right; a short hint need not cross a
  sanitizer red zone.
- [ ] Inspect logical value/display sizes, line offsets/lengths and iterator range
  without dereferencing an invalid range for logging. Distinguish this mismatch
  from absent-font or empty-line failures. Later cursor clamping cannot undo an
  earlier invalid read.
- [ ] Review the guard's placement: Cheryl returns before resetting
  `ideal_cursor_position_to_the_right_of_cursor`. Follow relative indices through
  absolute cursor/selection updates, especially later textarea lines. Correct
  this in upstream C++, with an upstream regression; the CMake source-copy helper
  remains downstream integration.
- [ ] Complete the cases below, run the selected regression and full upstream
  unit suite with AddressSanitizer/UndefinedBehaviorSanitizer, and check that the
  selected cases actually ran. Choose a compatible configuration from the pinned
  [sanitizer workflow](../../../extern/rmlui/.github/workflows/build.yml) and
  [unit-test target](../../../extern/rmlui/Tests/Source/UnitTests/CMakeLists.txt).
  Record compiler/standard library, font engine, backend and runtime mode;
  unexecuted configurations remain unverified.
- [ ] Check current contribution and [AI policy](https://github.com/mikke89/RmlUi/blob/master/AI_POLICY.md)
  before any external submission. The source analysis and Cheryl guard were
  AI-authored. A human submitter must personally understand, reproduce, review and
  test the change, author required human communication, and disclose the actual
  AI contribution/model as required. Do not repurpose an AI-written issue draft
  as human-authored communication. No upstream contact is part of this plan's
  maintenance.

### Required placeholder cases

Use the existing [input unit-test fixture](../../../extern/rmlui/Tests/Source/UnitTests/ElementFormControlInput.cpp)
and [text-input benchmark](../../../extern/rmlui/Tests/Source/Benchmarks/WidgetTextInput.cpp)
for public event submission, selection inspection and time advancement. The
existing `form.input.text.no_font` case covers a different failure.

| Case | Required observation |
| --- | --- |
| Short/long ASCII hints; left/middle/right clicks | Focus works; editable value, caret and selection endpoints remain empty/zero; no invalid access. The long case demonstrates pre-fix failure. |
| Drag, Shift-click and double-click on a hint | Hint never becomes selected or editable text. |
| Type, clear and click again | Only typed text becomes the value; clearing restores safe placeholder behavior, including retained cursor state. |
| Nonempty value; empty field without a hint | Existing caret placement and selection still work. |
| Multibyte UTF-8 hint and entered value | Hint stays outside the editable value; entered-text character boundaries remain valid. |
| Left, center and right alignment | Visible caret uses the empty value's aligned insertion position. |
| Wrapped/newline textarea hints; later-line clicks and vertical movement | No invalid range; final absolute caret/selection endpoints are zero and selected text is empty. |

The [test shell](../../../extern/rmlui/Tests/Source/Common/TestsShell.cpp) performs
font loading/layout with a dummy renderer by default; `RMLUI_TESTS_USE_SHELL`
selects its native backend. After logic/sanitizer checks, separately observe real
focus, visible caret, placeholder appearance and cleanup in native mode. A
headless pass does not establish those visual results.
