# Document the generated API

Use this guide when creating, repairing or maintaining API source comments and
their generated reference. Read the shared [documentation workflow](documentation.md)
first. Explain the public contract from the responsible declaration, implementation
and relevant tests; inspect what Doxygen, Breathe and Sphinx actually publish.

## Define the full coverage

Inventory the current public-header inputs, extracted entities and rendered
articles before a broad pass. The [generator](../site/generation/build.py),
[Doxyfile](../site/generation/Doxyfile.in) and
[Sphinx configuration](../site/generation/conf.py) establish publication scope.
Audit inventories provide the earlier baseline and priorities, not a permanent
list of current declarations.

Cover every meaningful exposed entity, including members that appear only inside
a type's article: classes, structs, unions, public/protected fields and methods,
constructors, destructors, operators, overloads, enum values, aliases, templates,
concepts, constants, public macros, callbacks and authored header/namespace content.
Check inherited members and specializations. A standalone page count does not
measure this coverage.

Review each entity's meaning, even when it needs little prose. A transparent alias
or self-contained constant may be complete with a brief explanation or an accurate
declaration and context. A policy field or lifecycle method usually needs much
more. Thorough documentation supplies all applicable meanings; repeating a name
or adding identical boilerplate to every symbol does not satisfy the request.

Prioritize common lifecycle, resource, input, text and other heavily used APIs,
then complete the remaining exposed scope. Prioritization changes work order,
not the final denominator. Preserve the declared private/internal boundary.
Mechanical namespace/file indexes need useful navigation, not invented contracts.
Accidentally published include guards and private visibility helpers are filtering
defects; address publication instead of explaining them as consumer features.

## Establish the contract before editing

Read the declaration, implementation, callers and relevant tests as needed to
answer the applicable questions below. Check virtual implementations and failure
paths before promising behavior on behalf of an interface. Existing ordinary
comments can hold valuable contracts: verify and preserve their meaning when
converting them into extracted documentation.

| Entity | Required meanings to consider |
| --- | --- |
| Type/interface | Purpose, supported use, invariants, default/empty state, ownership, copy/move behavior, synchronization and availability. Unions need active-member rules. |
| Function/method/operator | Inputs and defaults, units/ranges, result and ownership, side effects, publication timing, valid state/thread/context, failure and preserved state, and differences between overloads. |
| Constructor/destructor/reset | Acquired and borrowed resources, construction failure, empty/moved-from state, teardown prerequisites, idempotence where promised, and final release versus deferred native retirement. |
| Field | Meaning, units, coordinate/index domain, valid values, sentinel/default behavior, ownership/mutability and relationships to other fields. |
| Enum/value/flags | Purpose of the choice, each value's effect, valid combinations and unsupported/reserved values where defined. |
| Template/alias/concept | Parameter meanings, constraints, additional semantics beyond the spelling, specialization differences and relevant related types. |
| Constant/public macro | Meaning, units/domain, sentinel behavior, feature/build availability, and intended consumer or module-author use. |
| Callback/callable argument | Invocation phase/thread, synchronous or deferred execution, argument/capture lifetime, retained ownership, reentrancy, exceptions and unsubscription. |
| Header/namespace | Include purpose, required link targets/features and route to the owning guide. Document actual choices; avoid filler for a mechanical index. |

For operations, check these details explicitly when relevant:

- Null, empty, zero, negative, out-of-range and unknown values; whether input is
  copied, moved, borrowed or retained, and how long it must remain valid.
- Return values, references, views, handles, futures and optional results; what
  they own or borrow, when they become usable, and what invalidates them.
- Which thread/owner/context may call, what is synchronized, and when accepted
  requests become visible. Distinguish request acceptance from applied state.
- Allocation, validation and backend failures; exception types or error results
  that callers need, partial effects, retained prior output, and retry behavior.
- Mutations, callback invocation, resource-domain requirements, teardown ordering
  and behavior after move/reset/shutdown. State guarantees only when supported.

Use `@brief`, `@param`, `@tparam`, `@return`/`@retval`, `@throws`, `@pre`, `@post`,
`@note` and `@see` when they make the verified contract easier to retrieve. Omit
inapplicable sections. A parameter entry explains its meaning and restrictions,
not just its C++ type. Document `void` operations through effects and state.

Do not invent a stronger guarantee than source or an established interface
supports. An implementation defect, ambiguous contract or unsupported expectation
is discovered work requiring resolution, not an invitation to write plausible
prose. Keep internal algorithm commentary separate from public obligations.

## Keep class definitions compact

The repository preference is a scannable class body with detailed contracts outside
it. Follow the existing declaration order and keep data members above methods.
Do not move definitions, change signatures/access, or refactor a class to fit docs.

| Content | Preferred location |
| --- | --- |
| Type overview | Before the class/struct/union, outside its body. |
| Short field or enumerator meaning | After its declaration, using `///<` or `/**< ... */`. |
| Detailed member or method contract | After the complete class, in the same extracted header, using an explicit qualified Doxygen target. |
| Definition already outside its class in an extracted header | Before that definition when it attaches to the intended entity without a repeated target. |
| `.cpp` definition documentation | Only when that source is included in the extraction and cache inputs; the current pipeline excludes it. |

Doxygen supports trailing member comments and detached structural blocks. Keep
structural commands out of trailing blocks. See the official
[comment placement guide](https://www.doxygen.nl/manual/docblocks.html).

### Example placement

This fictional example illustrates locations, not a Cheryl API or a complete
implementation. Adapt contracts to actual behavior; the declaration list remains
short even when the method needs a detailed explanation.

```cpp
namespace Example {
    /** Owns bytes that callers can copy into their own output storage. */
    class Buffer {
    public:
        std::size_t max_copy = 0; ///< Maximum bytes per copy; zero permits no copying.

        [[nodiscard]] std::size_t copy_to(std::span<std::byte> destination) const;
    };
}

/**
 * @fn std::size_t Example::Buffer::copy_to(std::span<std::byte> destination) const
 * @brief Copies buffered bytes, bounded by destination size and max_copy.
 * @param[out] destination Writable storage that must not overlap the buffered bytes.
 * @return Number of bytes copied; zero when no bytes can be copied.
 *
 * The caller retains ownership of destination; this operation does not retain it.
 * Bytes beyond the returned count remain unchanged. Concurrent mutation of this
 * buffer requires caller synchronization.
 */
```

If a field needs more than a short trailing explanation, keep that explanation
short and place its additional contract outside the class, for example:

```cpp
/**
 * @var std::size_t Example::Buffer::max_copy
 * @details Changing the limit affects later calls, without resizing buffered storage.
 */
```

Maintain one detailed contract for an entity. Avoid splitting its rules among
several detached blocks or duplicating the same prose beside the declaration,
definition and manual. A useful short brief and one detailed owner can coexist.

### Associate detached blocks precisely

Use `@fn` with a qualified function declaration on one line, and `@var` with the
qualified variable declaration. Match argument types, template scope and relevant
qualifiers, including leading `::` inside template arguments; update targets
with signature changes. Prefer direct attachment when a definition already lives
outside its class. The official
[command reference](https://www.doxygen.nl/manual/commands.html#cmdfn) explains
these targets and their duplication risk.

Review constructors, conversions, overloaded operators, cv/ref-qualified methods,
constrained templates, partial specializations and inherited methods individually.
Do not create a phantom declaration or attach several overloads' contracts to one
member. Preserve real defaults, qualifiers, attributes and base types in output.

Describe each overload's actual differences with explicit links. Avoid order-based
"the overload above" wording and automatic `@overload` prose: current extraction
sorts member documentation. Use `@copydoc`, `@copybrief` or `@copydetails` only for
the same contract; identify overloaded targets by argument types. Add actual
override differences instead of copying incompatible lifetime or failure promises.
The current parser can fail on nested template return types; a verified
`@fn auto Qualified::method(...) -> ReturnType` target can attach where the
prefix-return form does not. Inspect XML even when extraction emits no warning.
For the two ref-qualified `StartupResult::make_runtime` declarations, detached
`@fn` matching loses the ref qualifier. Their concise trailing briefs and the
complete type-level lifetime contract provide the tested compact fallback.

See the [overload](https://www.doxygen.nl/manual/commands.html#cmdoverload) and
[copy commands](https://www.doxygen.nl/manual/commands.html#cmdcopydoc).

### Respect extraction inputs

Current `header_inputs()` selects public `.h`, `.hpp`, `.inl`, `.ipp` and `.tpp`
files under the configured include roots, plus the explicitly selected public
exception owner at `projects/engine/include/internals/exceptions.h`. Its historical
path does not authorize extracting other implementation headers.
`SEARCH_INCLUDES` is off and the extraction fingerprint hashes those headers. A comment only in a `.cpp` file will
neither enter the reference nor invalidate its extraction cache.

Doxygen can associate comments at an out-of-class source definition when that
file is an input. If implementation work adopts `.cpp` placement, treat selecting
the source files, cache invalidation, public/private filtering and source inventory
as one necessary tooling change. Check cold and incremental output and prevent
internal source entities from becoming accidental public articles. Moving comments
alone is insufficient. Keep the header form until the supported path is established.

Do not add filler file comments just to trigger extraction: the current Doxyfile
uses `EXTRACT_ALL=YES`. Write header documentation when it explains real include,
feature, linking or migration choices. Missing-documentation warnings are disabled;
an undocumented declaration can pass generation.

## Connect symbols to use

Give significant APIs a nearby route to the canonical manual section or maintained
example that shows their use. Link precise lifecycle/ownership rules when the
operation depends on them; retain essential preconditions locally. Link related
declarations explicitly with Doxygen references when they clarify a choice.

Before adding guide links throughout comments, verify one complete path through
Doxygen XML, Breathe and the generated HTML. A Markdown link relative to a header
is not necessarily resolved relative to that header in the final API article.
Use a supported, checked mechanism; keep links usable in the generated reference
and versioned documentation. Avoid guessed generated filenames or a pinned web
URL that disconnects the example from the displayed source revision.

Use the checked manual-link form, with the document name relative to `docs/site/`:

```cpp
/** @see [Font selection](cheryl-guide:assets/text-layout#font-selection) */
```

The repository-link extension resolves `cheryl-guide:` after Doxygen/Breathe to
a relative HTML link within the current documentation version. The document must
exist; normal generated-site checks verify its fragment. Use actual headings,
not guessed anchors. This form has a Doxygen-to-HTML pilot; ordinary source-relative
Markdown paths do not gain that interpretation automatically.

## Verify a batch and finish the scope

Before bulk comment placement changes, check representative cases: a trailing
field and enum value, a detached method and longer field contract, an overload,
a template/specialization and a virtual/inherited member. Use the actual repository
toolchain and permitted verification. Official Doxygen syntax support alone does
not prove correct association or rendering through Cheryl's full pipeline.

For each coherent subject batch:

1. Compare the current extracted/published entities with the source declarations.
   Account for every exposed member, including nested and inherited content.
2. Verify comments against implementation, then inspect XML association, parameter
   names, overload targets, qualifiers and cross references. Unmatched blocks,
   phantom declarations, accidental internal exposure and lost existing prose
   are failures even when some output looks correct.
3. Inspect the rendered type/member articles. Confirm that prose, parameter/return
   information, specialization bases, signatures, examples and guide links are
   accurate and findable. Do not compensate for a wrong generated declaration
   by adding a prose disclaimer; fix the owning extraction/rendering mechanism.
4. Walk a starting-reader use question and an expert contract question from the
   symbol. Require an explicit answer or useful handoff, not engine-source reading
   to fill a documentation gap.
5. Run the applicable permitted checks and generation, using the regular output
   described in the [tooling guide](../site/development/documentation.md). Report
   exactly what was source-inspected, extracted, rendered and executed.

Track coverage in the working context against the current entity inventory.
Account explicitly for an intentional exclusion or mechanically sufficient
declaration; do not count it as a fully documented complex contract. Keep the
historical usefulness report intact. A successful build, comment count or average
score cannot establish that every requested API entity has been covered.

API work is complete only when the full requested exposed scope is accounted for,
applicable contracts are supported and visible, declaration identity is preserved,
and relevant links/presentation checks pass. Unresolved semantic or rendering
defects remain outstanding work at handoff.
