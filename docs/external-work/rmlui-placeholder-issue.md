# RmlUi placeholder hit-testing: issue draft and investigation guide

This is a private, AI-authored study document, checked against the source and RmlUi's AI policy on 2026-10-08. The issue-shaped section below preserves the report and diagnosis; it is **not a body to paste into RmlUi's tracker**. Their [AI policy](https://github.com/mikke89/RmlUi/blob/master/AI_POLICY.md) requires human-authored GitHub communication. The guide explains how to investigate this yourself and write your own submission.

No upstream issue or PR has been opened. No build, test, debugger session, or independent crash reproduction was run for this analysis. The proposed fix and testing instructions have not been validated by execution.

## Issue-style study draft

### Title: Text input placeholder hit-testing uses display lengths with an empty editable value

Clicking an empty text input while its placeholder is visible crashes my application's demo. This started after replacing an editable initial `value` with the native `placeholder` attribute:

```xml
<input id="field" type="text" placeholder="Click here and type" />
```

The dependency is RmlUi 6.3, pinned to [`ba95ffe8bfb6370efb2cdcca927eaad4710c5413`](https://github.com/mikke89/RmlUi/commit/ba95ffe8bfb6370efb2cdcca927eaad4710c5413). The application loads its font, shows the document, and updates the context before forwarding input. The placeholder is displayed as expected. The application does not implement a custom text-input widget or manually set `placeholder-shown`.

Expected behavior: clicking focuses the field, the editable value remains empty, and typing replaces the help text with the entered value.

Observed behavior: the application crashes when the field is clicked with the placeholder showing. There is no captured backtrace or sanitizer report yet, and this has not been reproduced in a standalone upstream sample. The following diagnosis comes from source inspection; attribution of the reported crash to this path still needs runtime confirmation.

### Suspected failure path

The widget distinguishes the displayed text from the editable value, but its character hit-test mixes those two representations.

1. [`InputTypeText::OnAttributeChange`](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Source/Core/Elements/InputTypeText.cpp#L41-L64) forwards changes to the native widget. [`SetValueOrPlaceholder`](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Source/Core/Elements/WidgetTextInput.cpp#L1017-L1030) sets `placeholder-shown` and stores the placeholder in `text_element` when the value is empty.
2. [`WidgetTextInput::GetValue`](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Source/Core/Elements/WidgetTextInput.cpp#L1008-L1015) returns a static empty string while `placeholder-shown` is set. This keeps the hint out of the editable value.
3. [`FormatText`](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Source/Core/Elements/WidgetTextInput.cpp#L1285-L1296) generates line content from `text_element` and sets `line.editable_length` from that displayed content. It [retains those lines](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Source/Core/Elements/WidgetTextInput.cpp#L1403-L1409) and clamps the absolute cursor index, without converting their lengths to the empty editable value.
4. [The mousedown/drag handler](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Source/Core/Elements/WidgetTextInput.cpp#L693-L723) calls `CalculateCharacterIndex` before clamping the resulting cursor index.
5. [`CalculateCharacterIndex`](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Source/Core/Elements/WidgetTextInput.cpp#L1120-L1156) takes its starting pointer from `GetValue()` but its offset and length from the displayed line:

```cpp
const Line& line = lines[line_index];
const char* p_begin = GetValue().data() + line.value_offset;
const char* p_end = p_begin + line.editable_length;

for (auto it = StringIteratorU8(p_begin, p_begin, p_end); it;)
{
    ++it;
    // Width is then measured from this same range.
}
```

For the first line of the original ASCII hint, the mismatch is:

| Quantity | Value while the placeholder is shown |
| --- | --- |
| Displayed text | `"Click here and type"`, 19 bytes |
| Editable value from `GetValue()` | Empty, 0 bytes |
| First line offset | 0 |
| First line editable length | 19 |
| Supplied hit-test range | Empty value's `data()` through `data() + 19` |
| Valid editable insertion position | 0 |

That range does not describe the empty string. The [iterator constructor and increment](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Source/Core/StringUtilities.cpp#L608-L619) trust the supplied range. [Its validity check](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Include/RmlUi/Core/StringUtilities.h#L183-L184) checks pointer boundaries, and [UTF-8 seeking](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Include/RmlUi/Core/StringUtilities.h#L99-L105) does not stop at a NUL terminator. Incrementing can therefore read outside the empty value's valid character range and, depending on storage and how far iteration proceeds, outside its allocation/object.

This establishes a range mismatch in upstream widget code. It does not establish the exact instruction that crashed the application. Small-string storage, pointer position, and the measured widths can affect whether a particular run crashes or produces a sanitizer diagnostic.

### Possible fix

An empty editable value has only one valid insertion position. A candidate fix is to return zero from `CalculateCharacterIndex` before forming pointers or indexing the display lines:

```cpp
if (GetValue().empty())
    return 0;
```

This preserves the distinction between visible hint text and editable content. It also covers the [vertical cursor movement caller](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Source/Core/Elements/WidgetTextInput.cpp#L892-L924), rather than guarding only a mouse event. This is a proposed correction, not an executed or upstream-accepted fix.

The exact placement needs review alongside `ideal_cursor_position_to_the_right_of_cursor` and subsequent cursor/selection updates. Placeholder alignment already has [separate cursor handling](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Source/Core/Elements/WidgetTextInput.cpp#L1451-L1495). A regression should cover those behaviors and the shared textarea widget before claiming the guard is sufficient.

### Related reports and outstanding evidence

[Issue #868](https://github.com/mikke89/RmlUi/issues/868) concerns adding placeholder support. [Issue #1008](https://github.com/mikke89/RmlUi/issues/1008) concerns missing-font cursor failures. Both were closed when checked; neither establishes this placeholder/value range mismatch. Searches did not find a matching open issue or PR on 2026-10-08. That is not proof there is none; repeat the search before filing.

A complete report still needs the exact failing runtime configuration, a small unmodified-upstream reproduction, and a backtrace or sanitizer diagnostic linking the click to this range. It should also say whether the current upstream branch reproduces the problem. A fresh source check of [`master`'s hit-test method](https://github.com/mikke89/RmlUi/blob/master/Source/Core/Elements/WidgetTextInput.cpp#L1120-L1156) on 2026-10-08 found the same pointer/length construction; that is a source observation, not a runtime test.

## Understanding the issue yourself

### Trace the two strings before changing anything

Read the linked methods in this order: `InputTypeText::OnAttributeChange`, `OnPlaceholderAttributeChanged`, `SetValueOrPlaceholder`, `GetValue`, `FormatText`, then the mousedown handler and `CalculateCharacterIndex`. The [attribute callbacks](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Source/Core/Elements/WidgetTextInput.cpp#L250-L281) connect the markup to the internal state. Finish with `StringIteratorU8::operator++` and `SeekForwardUTF8`, so you can explain the actual read rather than assuming a NUL ends iteration.

The invariant required by the hit-test is:

```text
0 <= line.value_offset
0 <= line.editable_length
line.value_offset + line.editable_length <= GetValue().size()
```

When the placeholder is showing, line geometry describes the hint while `GetValue().size()` is zero. Displaying a nonempty hint is correct; treating its length as an editable range into the empty string is the problem.

The original field is a single-line example with offset zero. Wrapped textarea hints can have later lines with nonzero offsets, making the mismatch affect both pointer expressions. Check that case separately rather than assuming a successful input test proves every shared-widget path.

### Confirm the diagnosis in an isolated upstream checkout

Use a separate RmlUi checkout and build it as the root project, outside Cheryl's dependency setup. Start with the pinned 6.3 revision, then repeat on the current upstream revision and record its full commit ID. Preserve the shared `extern/rmlui` checkout.

There is already a Cheryl source-build workaround described below. A demo binary compiled with that workaround cannot demonstrate the unmodified upstream failure.

For a small upstream reproduction, load a valid font and use a document such as this. It is a suggested fixture, not a reproduction already run:

```xml
<rml>
<head>
    <title>Placeholder hit-test</title>
    <style>
        body {
            width: 600px; height: 150px;
            font-family: LatoLatin; font-weight: normal; font-size: 16px;
        }
        input {
            display: block; width: 320px; height: 34px; padding: 6px;
            background-color: #ffffff; color: #000000; tab-index: auto;
        }
        input:placeholder-shown { color: #78818b; }
    </style>
</head>
<body>
    <input id="field" type="text" placeholder="Click here and type" />
</body>
</rml>
```

RmlUi's test shell [loads the bundled LatoLatin fonts](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Samples/shell/src/Shell.cpp#L31-L50). In a standalone sample, explicitly load the font and verify success before using this family. Show the document and call `Context::Update()` before sending input. Confirm that the field has a valid font face, positive content width, and at least one formatted line. This avoids confusing the suspected bug with a missing font or an unformatted field.

Click at the left, middle, and right of the field. Forward `ProcessMouseMove` before `ProcessMouseButtonDown(0, 0)`, then release with `ProcessMouseButtonUp(0, 0)`. Keep individual trials separate in time unless double-click behavior is the case under investigation.

With a debugger, break in `CalculateCharacterIndex` and inspect:

- `parent->IsPseudoClassSet("placeholder-shown")`.
- `GetValue().size()` and `text_element->GetText().size()`.
- `line_index`, `lines.size()`, `line.value_offset`, and `line.editable_length`.
- `p_begin`, `p_end`, and the iterator position as `++it` enters `SeekForwardUTF8`.

The predicted state is a visible nonempty hint, an empty logical value, and a nonempty range built into that empty value. Capture the stack and these values at the invalid access. Do not manually dereference the bogus range just to print it.

[The later cursor clamp](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Source/Core/Elements/WidgetTextInput.cpp#L926-L943) can prevent an invalid final insertion index; it cannot undo an earlier invalid read. If your captured failure is instead an empty `lines` access or a missing font, record that difference and revise the diagnosis.

For AddressSanitizer, also try a much longer ASCII placeholder, for example 256 repeated characters, and click toward the right side of the field. Check the actual formatted line length. A longer range makes crossing the empty string's physical storage more likely; a short hint can read invalid logical content without crossing a sanitizer red zone. No fixed small-string capacity or guaranteed crash should be assumed.

### Choose and review the C++ change

Before implementing the guard, explain why zero is the right result for an empty editable value regardless of the hint's width, alignment, or mouse position. The visible hint remains available to layout and rendering.

Then follow the return value through `SetCursorFromRelativeIndices`, `MoveCursorToCharacterBoundaries`, `UpdateCursorPosition`, and selection updates. For a later textarea display line, a relative zero can still acquire a nonzero absolute offset before clamping. Verify that the final caret and selection remain zero.

Review whether the guard should precede or follow the reset of `ideal_cursor_position_to_the_right_of_cursor`. Cheryl's provisional guard returns at method entry, skipping that reset. Test transitions from an edited value back to the placeholder and double-click selection instead of assuming that retained state is harmless.

Simply substituting `text_element->GetText()` for `GetValue()` would make the range describe displayed text, but would also make hit-testing operate on the hint's character positions. Explain how the chosen fix preserves the empty editable value contract. Likewise, changing line lengths during formatting may affect display alignment and other consumers; do not change their meaning without following those uses.

Keep the upstream correction in C++ and its regression in the upstream test suite. The Cheryl CMake mechanism is downstream packaging, not the underlying fix.

## Regression and acceptance guidance

A useful regression exercises the public input path with a real font and completed layout. An assertion that an empty field remains empty is necessary but can pass despite the invalid read; pair behavioral checks with a pre-fix failing memory diagnostic or another demonstrable regression failure.

The existing [`ElementFormControlInput.cpp` unit-test file](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Tests/Source/UnitTests/ElementFormControlInput.cpp) is a natural place for a focused input regression. Its `form.input.text.no_font` case tests a different condition. The [text-input benchmark](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Tests/Source/Benchmarks/WidgetTextInput.cpp) shows public mouse-event submission, selection inspection, and manual time advancement.

Write the regression yourself using those fixtures:

1. Obtain the context from `TestsShell::GetContext()`, load the small document, and find its native `ElementFormControlInput`. Assert successful document/input creation, show the document, and update. Then assert a valid font face, an empty value, and `placeholder-shown` before sending events.
2. Derive pointer coordinates from the field's laid-out content box. Click and release through the context API, update, and inspect `GetValue()` and `GetSelection()`. Both selection endpoints should be zero and selected text empty.
3. Submit text through `ProcessTextInput`. Assert that only the typed text becomes the value and the hint disappears. Clear the value, update, assert the hint returns, and repeat the click.
4. Include a long-placeholder case that demonstrably fails against the unchanged upstream source. Run the same case against the proposed fix. Keep the regression when it passes with the fix.
5. Close the document and use the fixture's shutdown conventions. Advance the test system's manual time between separate clicks, as the benchmark does, to avoid accidentally testing double-clicks.

Use these cases to assess the shared helper:

| Case | Required observation |
| --- | --- |
| Original short ASCII hint; left/middle/right clicks | Focus works; value and selection stay empty; no memory diagnostic |
| Long ASCII hint | Demonstrable failure before the change, success after it |
| Drag, Shift-click, and double-click on hint | Hint never becomes selectable or editable content |
| Type, clear, then click again | Typed value is exact; clearing restores safe placeholder behavior |
| Nonempty value and an empty field without a hint | Existing caret placement and selection behavior remain correct |
| Multibyte UTF-8 hint and entered text | Hint stays outside the value; editable text preserves character boundaries |
| Left, center, and right alignment | Caret uses the empty value's aligned insertion position |
| Textarea with wrapped/newline hints; clicks on later lines and vertical movement | No invalid range; final caret/selection stay within the empty value |

The [test shell](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Tests/Source/Common/TestsShell.cpp#L16-L95) uses its dummy renderer by default while still loading fonts and performing layout. `RMLUI_TESTS_USE_SHELL` enables the native backend window. Start with the default test mode to isolate the widget logic, then perform native mouse QA. A headless pass does not replace checking actual focus, visible caret placement, and placeholder appearance.

### Suggested Linux sanitizer run

These commands are for a separate upstream checkout **after you have added the regression**. They are based on RmlUi's [Linux sanitizer workflow](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/.github/workflows/build.yml#L53-L84) and [unit-test target](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/Tests/Source/UnitTests/CMakeLists.txt). They have not been run here. Prerequisites include Clang, CMake with preset support, Ninja, FreeType, SDL2, SDL2_image, and OpenGL development dependencies.

Replace the absolute checkout path. Name your new cases with the `form.input.text.placeholder` prefix, or adjust the selector. The subshell preserves the caller's working directory.

```bash
(
    set -eu
    rmlui_checkout="/absolute/path/to/separate/RmlUi"
    cd -- "$rmlui_checkout"
    cmake -S . -B Build-placeholder-asan -G Ninja --preset dev \
        -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=OFF \
        -DRMLUI_FONT_ENGINE=freetype -DRMLUI_BACKEND=SDL_GL2 \
        -DRMLUI_SDL_VERSION_MAJOR=2 \
        -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
        -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
    cmake --build Build-placeholder-asan --target rmlui_unit_tests --parallel
    env -u RMLUI_TESTS_USE_SHELL \
        ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
        UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
        LSAN_OPTIONS="suppressions=$rmlui_checkout/Utilities/lsan.supp:print_suppressions=1" \
        ./Build-placeholder-asan/rmlui_unit_tests \
        --test-case="form.input.text.placeholder*"
)
```

First run with the upstream C++ unchanged and the new regression present. Save the failure and its stack. After applying your correction, repeat the same configuration/run; a matching existing build directory can be reused. Check that the selector actually executed your cases: an empty selection is not a passing regression.

Then run the complete `rmlui_unit_tests` executable with the same sanitizer environment and without the `--test-case` filter. For visual QA, use an upstream sample or the test shell's native mode with a display available. Record which revision, compiler/standard library, font engine, backend, and runtime mode were exercised.

Use the supported [upstream CI configurations](https://github.com/mikke89/RmlUi/blob/ba95ffe8bfb6370efb2cdcca927eaad4710c5413/.github/workflows/build.yml) to choose additional relevant environment coverage. Different standard libraries can expose this storage bug differently. Distinguish observed results from portability reasoning; unrun CI jobs are not evidence that the change works elsewhere.

## Cheryl context and the provisional workaround

The local report followed Cheryl commit `9fa1b75f8ad10602bd8861bbbb1058f4569aec3b`, which replaced an initial value with the native placeholder. The [demo field](../../assets/graphics/ui/demo.rml#L48) and [placeholder style](../../assets/graphics/ui/demo.rml#L25) show the resulting markup; the style only changes its color.

The [demo setup and update](../../projects/apps/demo/src/rmlui-demo.cpp) load a font, obtain a native `ElementFormControlInput`, show the document, and call `update_time` before handling input. The [session](../../projects/modules/ui/rmlui/src/session.cpp) updates the native context and forwards mouse movement and button events through the normal RmlUi API. Each complete tick batch is now handled once, with application controls before individual records. These observations make a stock-widget failure plausible; they do not rule out every integration fault without a reproduction and stack.

Cheryl commit `f0a25b2203fcf809601ed8009c8964d8b3bedfef` adds the [source-build workaround](../../projects/modules/ui/rmlui/cmake/RmlUi.cmake). The [module dependency contract](../../projects/modules/ui/rmlui/README.md#placeholder-dependency-contract) owns generated-source behavior and correction declarations. The guard changes C++ behavior; CMake arranges which source is compiled while leaving the dependency checkout unchanged.

The helper applies only when Cheryl creates the RmlUi source target. Existing `RmlUi::Core` targets and installed packages bypass it, but the [module](../../projects/modules/ui/rmlui/CMakeLists.txt) now rejects them unless they declare `CHERYL_RMLUI_PLACEHOLDER_FIX=TRUE` or the consumer attests through `CHERYL_RMLUI_PLACEHOLDER_FIX_VERIFIED=ON`. This declaration must describe a correction already present in the supplied library; it does not install or execute the guard. Neither dependency path has been runtime-validated for this report. Do not describe the local guard as a confirmed fix or use a guarded Cheryl build as the pre-fix upstream baseline.

## Preparing a submission under RmlUi's AI policy

The [policy](https://github.com/mikke89/RmlUi/blob/master/AI_POLICY.md) says:

> All *communication* on the RmlUi GitHub repository must be written by human authors.

This includes issues, PR descriptions, and comments. Its exceptions are faithful translation and limited, attributed AI quotations with human-written context and predominantly human-authored content. Disclosure does not make an AI-generated issue body acceptable.

For code, the policy permits smaller AI-assisted PRs subject to disclosure of extent, model, and AI-written parts. The author must understand, review, and test the code, take responsibility for it, and be certain it works in other environments. Large PRs mostly or entirely authored by AI are not accepted. Re-read the live policy before submitting.

The following is practical preparation, rather than additional rules imposed by that policy:

1. **Establish firsthand evidence.** Reproduce the failure yourself, capture the relevant state and stack, and determine whether it persists on current upstream. Record actual environment details and results. If attribution remains uncertain, say so.
2. **Explain the defect unaided.** Be able to identify the two strings, explain where the line length originates, show why the iterator's supplied end is wrong, and explain why clamping afterward is insufficient. Also explain why a short hint can appear safe in one environment.
3. **Review the correction yourself.** Understand every changed line and relevant caller, including the state skipped by an early return. Demonstrate pre-fix failure and post-fix success, check the neighboring text-editing behavior, and justify the environment coverage.
4. **Write your own GitHub communication.** Use your observations and understanding to author the issue, PR description, and replies. Do not paste this draft, ask AI to polish it for submission, or treat superficial edits as human authorship. If you quote a small portion, follow the policy's attribution and context requirements.
5. **Disclose AI involvement in your own words if submitting assisted code.** This analysis and the provisional Cheryl guard were AI-authored. If you use the suggested guard, identify that contribution, along with any AI-written tests or later changes. Record the actual model identity from the Codex session/settings rather than guessing; describe what you personally reviewed and tested.
6. **Keep unsupported claims out of the submission.** Include only runtime results you obtained. If you cannot yet meet the PR's understanding, testing, or environment requirements, continue investigating or open a human-authored issue with the limits stated instead of presenting the correction as accepted evidence.

The useful handoff to maintainers is a small reproducible failure, the violated range invariant, and independently checked behavior after a narrow change. This file preserves the investigation needed to reach that point; that firsthand verification and human-authored submission remain to be done.
