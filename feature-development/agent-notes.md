# MAGIC-M1 detailed engineering record

## Assignment and source boundary (before implementation)

Fresh worker-3 MAGIC task, not the archived map-storage queue. Starting HEAD
3542f883391d4225329dab8d5d61a5987e69c8f0, clean feat/magic. No existing
feature-development records or native outputs. The workspace worker record
runtime/workers/worker-3/status.json identifies run 20261024T024553Z-e2b3aa5a6271,
thread 01a16bcc-ecd0-7530-bace-9958eddb5e0a, reasoning_effort ultra and admitted
resource scope. Model selection is inherited; no substitutions. Workspace rules,
autonomy policy, worker assignment, backlog, brief, MAGIC design and decisions
are inspected. The three exploration reports are 27 alternative strategies,
not branches, experiments or passing implementations. Selected synthesis owns
fixture numbers and M1 scope. No sibling merge, dependency or mutation.

## Proposed ordinary caller (recorded before dependent APIs)

```cpp
Magic::Catalog game; // host supplies supported primitive schemas and keys
// Populate game with Heal -> game.heal, Fire -> game.fire, Slow -> game.slow.
auto result = Magic::compile(edited_text, game);
if (result) {
    accepted = *result; // shared_ptr<const Plan>, replace only on success
    std::cout << accepted->common() << '\n' << accepted->inspection();
} else {
    std::cerr << result.error().message; // owns code, span, excerpt and advice
}
// accepted survives edited_text, result and catalog destruction.
```

Private proposed names, not a public Engine API or user preference. Const plans
expose typed meaning; compilation/inspection never calls a host or executes effects.
Controlled Common is a grammar, not arbitrary English. Source selects semantic
primitives; only host schemas map those primitives to symbolic game keys.

## Literal normalized baseline (recorded before implementation)

All spatial values use named world frame G0, integer mm; time is integer 10 ms
ticks. Actor-centre disk boundary is inclusive. These are local prototype units.

| Fixture | Independent expected semantic values |
| --- | --- |
| S1 | spoken/release, contact, onset 0 ticks; binding hand = one willing touched actor selected at release, no fallback; [game.heal 12 hp, instant, hand]. |
| S2 | spoken/release, burst, onset 100 ticks; binding near = eligible actors in G0, disk radius 250 mm inclusive; aim captured at release, membership queried once at activation; ordered together [game.fire 8 dp instant near, game.slow 750 permille for 200 ticks near]. |
| S3 | spoken/release, burst, onset 0 ticks; same 250 mm near query; [game.slow 750 permille for 200 ticks near]. Exact contextual 25 framebuffer px at 10 mm/px, C7 revision 3, plane G0 has equivalent world meaning; absent/unsupported context refuses. |
| S4 | identical meaning to S2; Cinder@1 postfix fire and Mend@1 verb-first slow clauses cross explicit on near bridge boundaries. Mend uses recipient marker tava-a; Cinder slow additionally has duration-first order. |
| S5 | engraved/item-touch, exactly one required charge (metadata only), item-event delivery, onset 0; binding toucher = willing accepted-event toucher at activation; [game.heal 12 hp instant toucher]. No installed item, delivery, charge consumption or persistence. |

Independent Common literal readings live with test fixtures; canonical rendering
uses integer milliseconds (1000 ms onset, 2000 ms slow). Seconds are accepted
exactly, never rounded. Common round-trip promises equivalent typed meaning with
the same host catalog; source language, spans and px conversion provenance are
excluded from semantic equality. Author order and target binding identities remain
observable. Sequential then and conflicting duplicate fields refuse.

## API evidence and confidence

- Intentional v3 source eb3f81c (V3.0a #12), input-binding.h: separate digital and
  analog role types with explicit constructors; input-mapper.h explicitly calls
  its own purpose tentative ("I think. 99% sure"). Evidence for explicit roles,
  counterevidence against treating every imported abstraction as settled taste.
- Josh Cooper commit 4e40aa1cb309808ff7d9976c61228d1db7445e01, math/time.h:
  chrono dimensions and compile-time cast constraints. Supports distinct units;
  does not select our mm/tick numbers or justify lossy conversion.
- Josh Cooper commit 18eac46e8e070ad79554f459943462fb26442093, templates/block.h:
  shared owners and optional result with explicit bounds; evidence for lifetime
  reasoning, not a mandate to reuse that container or its public mutable fields.
- Modern CMakeLists.txt selects C++23; .clang-format owns formatting. Current
  first-game main.cpp is a compatibility example, not proof of personal taste.
- Storytelling U9 4f5e7eab4af961d95f5a04d6f4f079517e39a758 read via git show
  (dialogue.h, dialogue-session.cpp): immutable copied definitions, symbolic keys,
  owned errors/observations and live condition recheck. Prepare-before-effects
  helps later host design. Its graph is not a spell VM, its receipt is not durable,
  callback exceptions can leave application effects partial. No import or merge.

## Implementation sequence and acceptance map

1. Own syntax -> typed validation -> immutable meaning; render exact Common and
   role/key inspection. Malformed/ambiguous text must not replace previous results.
2. Two invented school grammars and explicit lexical binding; immutable artifacts
   must survive all input owners. Quantity/time/delivery/binding edits need witnesses.
3. Headless stdin/file inspection plus focused independent literal tests and small
   ordinary caller scenarios, not helper-only assertions.
4. All configure/build/native executions use workspace scripts/build --, one job,
   incremental clone-local build output, complete saved logs. Record refusals and
   stop native admission attempts on unchanged resource/allowance blocks.
5. Review staged diff, update compact/detailed records, next action and JSON receipt;
   local command-identity commit. Publisher owns remote backup.

## Reversible scope decisions

M1-013: explicit nonrecursive envelope and flat bindings, one activation and at
most four ordered effects. Common@1, Mend@1 and Cinder@1 are trusted handwritten
microgrammars. All school names/words are invented, not Elvish. Future language
packs need their own checked grammar; no provider/parser network calls.

M1-014: contact and instantaneous ray delivery can address the same touched-actor
query. This small additional delivery lets an edit change only delivery meaning;
the willing/touched predicate remains mandatory, ray path/occlusion is a later
host check. Burst accepts disks and item-event accepts an engraved event toucher.
No projectile travel, arbitrary combinations, scheduler or general World.

M1-015: limits are caller-tightenable below fixed ceilings. Source, tokens,
brace nesting, bindings, clauses/effects, numeric ranges, work and retained logical
storage have independent checks. Retained storage is deterministic object slots
plus owned text bytes including terminators, not allocator bookkeeping. Fixed
ceilings also bound vector/string capacity and temporary parsing storage. One
owned diagnostic has a separate small fixed reserve so a zero budget can explain
its failure. Exact numerical details and executed evidence follow after checks.

During grammar implementation, S4 uses Cinder fire plus Mend slow to make the
normalized S2 payload explicitly bilingual across two schools. The numeric and
target baseline is unchanged. Mend luma is a verb-first finite slow production;
Cinder neth remains duration-first. Neither is unrestricted lexical substitution.

## Implemented private contract

`projects/magic` is a standalone C++23 project using the standard library. It
changes no Engine target, sibling, submodule pin or dependency checkout. The
compiler has no provider, network, random, world, scripting or effect callback
path. `magic_inspect [VERSE_FILE]` accepts an editable file or stdin and emits
inspection plus exact Common; it rejects input above 8192 bytes while reading.
Its fixture host supplies `game.heal`, `game.fire` and `game.slow`. Other callers
supply their own Catalog. `tests/caller.cpp` uses distinct host keys and ordinary
calls, without the implementation or contract-test helper routines.

The compiler lexes checked tokens/spans, parses a private borrowed `Authored`
syntax tree, then validates dimensions, typed roles, binding references, host
schemas and combinations while constructing owned `Meaning`. Only successful
compile exposes a `shared_ptr<const Plan>`. The Plan constructor is private;
copy/move construction and assignment do not offer mutation. Source, binding/frame
names, symbolic keys, grammar/span provenance, conversion provenance, exact Common
and inspection are owned. Accessors borrow the plan's lifetime; copying its handle
retains everything after source, catalog, options and ephemeral compiler destruction.
Stable reads are concurrent; inputs must remain stable during each synchronous
compile. Validation failures own one diagnostic. Allocation exceptions propagate
without modifying inputs or previously accepted handles. Host permission is never
conferred by compiling a word or retaining a plan.

Entry medium (`Spoken`/`Engraved`), casting mode (`Release`), trigger
(`None`/`ItemTouch`), charge requirement, delivery, onset, query/binding timing and
effect duration are separate typed data. Release is the only supported mode;
wind-up, channels, enchantment envelopes and richer triggers need later contracts.
Engraving explicitly reads `engraved item-touch charges 1 release`. The shared
payload is independent of entry media. No item is installed and no charge/event
is consumed, replayed or persisted.

### Grammar and exact readings

ASCII case-sensitive words and punctuation, with whitespace between quantities and
units. `NAME` is 1..32 bytes, starting with a letter, then letters/digits/_/-.
Each `Q` below is an unsigned decimal with at most three fractional places plus a
unit; typed validation rejects the wrong dimension and any inexact normalization.
There are no aliases for arbitrary English, name guessing, macros or recursive
language islands.

```text
verse := entry via delivery after Q { binding+ together ordered { clause+ } }
entry := spoken release | engraved item-touch charges 1 release
delivery := contact | ray | burst | item-event
binding := bind NAME = willing touched actor at release ;
         | bind NAME = willing event toucher at activation ;
         | bind NAME = eligible actors in FRAME disk Q around aim captured
                   at release queried at activation inclusive ;
clause := on NAME bridge LANGUAGE @ 1 { payload } ;
Common payload := heal Q instant
                | fire damage Q instant
                | multiply speed by Q for Q
Mend payload := sela Q tava-a | luma Q tava-a lasting Q
Cinder payload := Q hara | Q Q neth
```

Mend `sela` is healing; `luma` is finite speed scaling. `tava-a` marks the actor
recipient exported by the outer bridge, including an actor set. Cinder `hara`
is fire with quantity before the verb; `neth` has duration first, rate second,
verb last. These invented subsets differ in word order and role markers; the
parser does not replace strings into Common. S4 combines Cinder fire and Mend
slow and equals the independent S2 Common meaning. `mixed-schools.verse` composes
a previously unauthored delayed healing-plus-slow payload. A healing disk also
compiles without a named whole-spell handler.

During contract review Common becomes explicitly `fire damage` and `multiply
speed by N permille`, avoiding ambiguity between a multiplier and a reduction.
750 permille means 0.750 times prior speed, not a 750-permille subtraction. Effect
impulses have duration zero; status duration never repeats fire or changes onset.
The normalized numeric baseline remains unchanged. Independent canonical strings
in `tests/literals.h` and full typed literals in `tests/compiler.cpp` exercise all
five fixtures. Common reparse under the same host catalog promises equal Meaning;
provenance and original spelling are intentionally separate. Authored effect order,
binding names/indices, queries, frame, radius and timing remain semantic data.

### Supported combinations and authority

| Entry and delivery | Query and timing |
| --- | --- |
| Spoken release, contact or ray | One willing touched actor per binding, selected at release, onset zero. Ray is immediate; real ray/contact/occlusion checks remain host work. |
| Spoken release, burst | Eligible actors in a named 2D world disk, inclusive actor-centre radius; fixed aim captured at release, membership queried once at activation; onset 0..8,640,000 ticks. |
| Engraved release, item-event | Willing accepted-event toucher, resolved at activation; exactly one required charge, onset zero; metadata only. |

Effects in one binding share one selection. Separate bindings keep independent
identity and may overlap; no disjointness or live acquisition is inferred. Tests
show both independent identities with identical queries and routing slow to a
second 500 mm query while fire retains its original 250 mm query. Reversing the
fire/slow clauses changes effect order and exact Common. `then` rejects explicitly.
On one binding, repeated speed fields or multiple health fields (including
heal+fire) reject as unsupported interactions. Distinct bindings require later
live overlap/conflict checks. Nothing silently treats ordered together as a
sequence or grants atomic external effect delivery.

Host schemas bind each known primitive to one unique symbolic key, 1..64 ASCII
letters/digits/._- bytes. At most three schemas, unique by both primitive and key;
unknown enum values reject. Catalog capability flags govern medium/delivery and
multi-effect bundles, and per-schema quantity/duration maxima can tighten bounds.
Unknown words, unsupported recognized operations and unsupported combinations
have distinct diagnostics. Frame names describe required host roles; M1 never
looks up a real frame, actor, aim point, eligibility policy or live item. Later
hosts must honor primitive meaning and recheck current support and permission.

`px` is specifically framebuffer pixels. The optional authoring context needs a
matching world frame, named camera/revision and plane, a positive bounded rational
mm/pixel scale and an explicit uniform orthographic assertion. Perspective,
nonuniform, mismatched, missing or inexact contexts refuse. C7 revision 3, G0,
10/1 mm per px is supplied only in caller checks, never a compiler/CLI default.
25 qualified px equals 250 mm; the plan freezes world radius and owned provenance.
Common emits mm, so reparsing resolved meaning needs no camera. Context and draft
mutation do not change an accepted plan.

### Bounds and diagnostic contract

| Dimension | Fixed ceiling / accepted numeric domain |
| --- | --- |
| Source | 8192 bytes, including whitespace; lexer checks before scanning. CLI bounds reads too. |
| Tokens / brace nesting | 512 / 8; no recursive parser or unbounded expansion. Current valid envelope depth is 3. |
| Bindings / clauses / effects | 8 / 4 / 4, checked before pushing syntax/storage. One effect per bridge clause, at least one binding and effect. |
| Work | 65,536 deterministic meter units; scanned bytes, token operations, numeric digits, schema/reference/composition checks and owned copying/rendering are charged. Other per-step helpers are bounded by fixed identifier/schema/numeric widths. This is a work model, not CPU cycles or a real-time deadline. |
| Retained logical storage | 32,768 bytes: sizeof Plan + binding/effect/provenance element slots + owned text bytes and terminators. Checks precede copying, reserve and output append. Excludes unused container capacity, shared control block and allocator bookkeeping; sizeof makes totals ABI dependent. Fixed ceilings bound those extra allocations and temporary tokens/AST too; no portable exact heap/RSS ceiling is claimed. |
| Healing/fire / disk radius | Positive integer 1..1,000,000 hp/dp/mm, also limited by host schema for effects. |
| Speed multiplier | Integer 1..1000 permille, also host-limited; duration 1..8,640,000 ticks. |
| Onset | Integer 0..8,640,000 ticks (10 ms each), further constrained by delivery. |
| Context scale | Positive integer numerator and denominator, each <=1,000,000; revision is nonzero uint32. |
| Diagnostics | One owned code, original byte offset/length, excerpt <=64 bytes and fixed repair message (current messages <=200 bytes). Separate fixed-size error reserve, including when work/storage budget is zero; EOF/global errors can have zero-length spans. |

Limits can only tighten ceilings, including zero. The lexer and parser stop at the
first error; diagnostic order follows syntax before schema/lowering. All numeric
multiply/add checks happen before arithmetic; exact rational cross-cancellation
precedes multiplication and division. Slot-size multiplication uses counts already
bounded to 8/4. Quantities are validated before any size could depend on them.
No geometry/array allocation is derived from an authored radius or duration.

Tests accept and reject both sides of every configurable budget, including zero.
Absolute 8192-byte input, eight bindings and four effects/clauses have valid
witnesses. Token 512 and nesting 8 also have lexical boundary witnesses that then
fail grammar; valid sources use tightened exact token/depth boundaries. Numeric
maxima and one-past/zero/inexact/overflow cases are exercised. For the canonical
S2 literal on this ABI, exact work=2610 and logical storage=2700 pass; one less
fails with the specific budget code. This differs from source fixtures because
whitespace, language and millisecond spellings affect metered text.

## Executed verification and saved complete logs

All commands run from this clone. Every configure/build/native test passes through
`/mnt/projects/cheryl-features/scripts/build --`; no parallel child builds or sibling
caches. All configurations reuse `projects/magic/build` incrementally. No lock
cancellation, resource refusal, allowance workaround, download or install occurs.
GNU 16.2.1 on GNU/Linux, CMake 4.4, Python 3.14.7; Debug, C++23, -Wall -Wextra
-Wpedantic -Werror, ASan+UBSan. Generated flags confirm sanitizer instrumentation.
Leak checking remains unverified because LSan refuses tracing; final test command
disables leak detection locally and makes UBSan fail immediately on a finding.

| Actual command | Complete log(s), result |
| --- | --- |
| `/mnt/projects/cheryl-features/scripts/build -- cmake -S projects/magic -B projects/magic/build -DCMAKE_BUILD_TYPE=Debug` | `magic/logs/configure.log`, `configure-02.log`; both exit 0. Second configuration registers the CLI check and discovers existing Python; no download. |
| `/mnt/projects/cheryl-features/scripts/build -- cmake --build projects/magic/build --parallel 1` | `magic/logs/build-01.log` through `build-05.log`; all five exit 0. Changes clarify Common, format code, make mode/trigger explicit and update affected literals. |
| `/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir projects/magic/build --verbose --parallel 1` | `magic/logs/checks-01.log`; exit 8, 8 LSan tracing failures. No acceptance inferred. |
| `ASAN_OPTIONS=detect_leaks=0 /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir projects/magic/build --verbose --parallel 1` | `magic/logs/checks-02.log`; exit 0, initial 8/8 CTest entries. Superseded wording/field layout. |
| `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir projects/magic/build --verbose --parallel 1` | `magic/logs/checks-03.log`; exit 8, 8/9 entries. One S5 literal omitted its newly explicit ItemTouch field and is corrected to the original item-touch baseline; compiler behavior is unchanged by that correction. Same command in `checks-04.log` exits 0, **9/9 entries**. |

Final acceptance: **35 contract cases**, **4 independent ordinary caller scenarios**,
**6 direct file CLI fixtures** (five normalized plus novel mixed healing/slow), and
**9 CLI process scenarios** (stdin parameter/delivery composition, exact/above byte
ceiling, owned unknown/ambiguous diagnostics, help, missing file, excess arguments).
These comprise 9 CTest entries; counts are separate categories, not 54 CTest tests.
All five independent full Meaning and Common literals pass compilation, inspection
and round-trip. Parameter, bilingual, scope/order, ownership, diagnostic, operation,
unit/dimension/range and exact budget cases pass. No sanitizer findings appear in
the final logs. Static format, staged whitespace and receipt/evidence checks are
recorded in `magic/logs/static-review.log` and `staged-review.log` at finalization.

| Actual fixture source | Tokens | Depth | Work | Retained logical bytes |
| --- | ---: | ---: | ---: | ---: |
| S1 | 35 | 3 | 1839 | 2024 |
| S2 | 64 | 3 | 2590 | 2693 |
| S3 | 50 | 3 | 2211 | 2289 |
| S4 | 61 | 3 | 2545 | 2670 |
| S5 | 38 | 3 | 2004 | 2147 |

## Handoff and limits

M1 is ready for coordinator review, not production casting. No immediate fake host
is included: compilation/inspection gives the coherent proof without pretending
delays, live targets, charges or gameplay effects execute. Real target acquisition,
scheduling, mana/balance/permissions, overlap/interaction laws, event authenticity,
charge commits, durable state/restart and exactly-once external effects are pending.
Graphics, learner progression, a spellbook/tome UI and fun/comprehension playtests
are unexecuted. Windows/macOS, other compilers/ABIs, installed Engine SDK and full
Engine builds are unverified. Words are invented; no actual Elvish grammar or
external language pack is implemented. Private syntax/API choices remain narrow
and reversible. Public documentation generation is outside M1 and is not run.

Next evidence: coordinator reviews this compiler receipt and selects a bounded M2
reference-host task, starting with explicit live selection/recheck, deterministic
simulation advancement and whole-step conflict policy. Ray scope earns only an
independent delivery-edit witness here; its next evidence is an actual contact/ray
eligibility/occlusion refusal in that host. No M2 code begins in this unit. The worker
ends idle; the coordinator publisher owns ordinary feat/magic backup pushes.

# MAGIC-M2 — proposed host contract before implementation

Starting clean at 3980904cf9fc03fac913d1fafd56c6f17d677b4e on feat/magic.
Run 20261024T035059Z-885902496b58, same assigned worker/thread; runtime record
confirms configured ultra reasoning and admitted memory scope. Current assignment
splits runtime spoken execution from later item/event/checkpoint work. S5 remains
compilable/inspectable and runtime-refused. No integration/sibling changes.
M1 contracts and intentional v3 provenance above remain evidence; recent compiler
APIs are compatibility constraints, not a new claim about personal API taste.

Proposed private caller, recorded before adding host APIs:

```cpp
auto host = ReferenceHost::create(current_game_catalog, limits);
auto caster = (*host)->add_actor(caster_spec);
auto recipient = (*host)->add_actor(recipient_spec);
auto plan = Magic::compile(edited_verse, compiler_catalog);
auto preview = (*host)->preview(*plan, {*caster, *recipient, std::nullopt});
auto ticket = (*host)->prepare(*plan, {*caster, *recipient, std::nullopt});
auto released = (*host)->release(*ticket); // rechecks live state/schema, then adopts whole step
// For burst: request has a world-mm aim point instead of contact.
auto observed = (*host)->advance_to(100); // explicit ticks, no service or wall clock
```

M2-016 (proposed): finite private host, actor slot+generation identities, immutable
owned reports, one host-incarnation-bound prepared ticket. A new preparation
invalidates the old ticket; successful release/discard consumes it. Ordinary
world changes make a preparation stale; live schema/permission/resource/contact
checks run before revision comparison for precise actionable refusal. Failed
release keeps the token and game state intact; a successful/repeated or foreign
input refuses. Preview issues no token and changes nothing. Host edits are trusted
fixture-owner operations, not security credentials for arbitrary native callers.

M2-017 (proposed): release costs 10 mana per effect, paid once by the caster.
Delayed release reserves one pending slot, captures the accepted world point and
retains the immutable M1 plan handle. Current members are queried on activation;
no future target/status capacity is promised. Cancellation frees the scheduled
slot, with no refund after accepted release. Advance stages the complete finite
step; any due refusal leaves clock, actors, resources and queue unchanged. There
is no automatic retry. Owner must correct the cause or explicitly cancel that
action. Same-tick activations run by increasing accepted action ID, with status
expiry first. This is deterministic reference ownership, not external delivery.

M2-018 (proposed): resolve every binding before writes. Any two writes to the same
actor/health field or actor/speed field in one activation refuse, including across
distinct bindings; binding names and actor storage order do not decide mechanics.
An active slow refuses any replacement/stacking; expiry restores multiplier 1000
and the next activation at that tick may apply a new one. Eligibility is frozen
for the activation before effects; health clamps to [0,max], status applies to
those selected even if simultaneous damage reaches zero. Later queries exclude
dead actors. These are reversible fixture laws, not balance or production combat.

Planned bounds: 16 actor slots, 8 pending casts, 16 active slows (one per actor),
128 selected (binding,actor) pairs and 1024 selector visits per complete step;
16,384 charged work units; one retained prepared plan plus up to eight pending
plans, each <=32,768 M1 logical bytes. World frame G0 only, positions +/-1,000,000
mm, contact centre distance <=50 mm, integer health/mana/base speed <=1,000,000;
clock, action/ticket/revision/generation counters have explicit tightenable caps.
World/scratch/report collections are fixed arrays; plan handles avoid deep copies.
Allocation for host/catalog/report occurs before nonthrowing adoption and may
throw without changing the existing host or ticket. Exact limits and executed
boundary evidence will be recorded after implementation.

## M2 implemented contract and evidence

The private native project now links `magic_reference_host` to the unchanged M1
compiler. `magic_reference_demo` is a small ordinary caller that compiles actual
fixture files, previews/prepares/releases them, advances simulation explicitly,
edits real actor positions and reads actual snapshots. It executes S1, S2, S3,
S4 and the novel mixed-school heal+slow spell. It prints S5's actual immutable
inspection, then verifies `UnsupportedEntry` and unchanged state. There is no
provider/network, random interpretation, Engine/sibling dependency, physics,
background loop, arbitrary callback or external effect path.

M2-016–018 are implemented as proposed above. The API is intentionally private,
with one reference owner and explicit edits. This is a selected engineering
experiment, not evidence of a personal preference inferred from agent code.
Intentional v3 evidence and its confidence/counterevidence remain in the M1 record.
No new personal API inference or sibling inspection is needed for this unit.

### Identity, ownership, authority and adoption

`ActorId` is a host-local slot (0..15) plus nonzero generation. Removal preserves
the slot generation; the next add increments it. Creation chooses the first free
slot; exhaustion of that slot's generation refuses rather than silently finding a
new identity policy. Actor references are logical host-local values, not global
credentials. Add/update/remove/catalog replacement and cancellation are trusted
fixture-owner operations; this library is not a sandbox against arbitrary native
code. Position, health/mana/base speed and permission/eligibility/willingness are
owned finite values. No external actor pointers or live camera references exist.

`Ticket` has private fields: a retained host-incarnation marker plus monotonic
serial. Default/foreign/superseded/consumed tickets refuse. Copying a token does
not create another use; host destruction cannot make its token valid in a new
host. There is at most one prepared record, retaining the plan and value request.
Successful new preparation supersedes the old token. Failed preparation preserves
the prior record (including ticket-exhaustion refusal). Preview issues no token,
changes no world/revision/resource/action and has no release authority. Delayed
preview validates current support and resources without promising future targets,
status capacity or a free queue slot.

Every release revalidates caster generation/aliveness, can_cast permission,
contact/aim shape, current schemas and available release mana before comparing the
prepared world revision. Any ordinary owner edit, successful release/cancellation
or time advancement changes revision and makes old preparations stale. Live
permission/resource/contact/schema failures have specific diagnostics before the
stale check. Failed release preserves the prepared input and complete world;
successful release/discard consumes it. Irrevocably stale input needs a fresh
prepare. Discard changes only prepared authority, with no gameplay charge.

Plans are the actual `shared_ptr<const Magic::Plan>` artifacts from M1. M2 copies
no source, compiler tree, key collection or Common text into runtime programs.
Prepared/pending records retain handles; bounds check the M1 logical retained-byte
metric before taking a runtime reference. Removing the final queue reference can
destroy the original plan after all source/catalog/compiler owners are gone.
Observations own fixed value snapshots and action events, and retain no plan or
host pointers. Const observation handles remain usable after later steps and
host/plan destruction. Snapshots returned by value can be edited without changing
the host. Callers own any observations/tickets they retain; the host does not keep
an unbounded output history.

Current schema validation compares **both exact symbolic key and primitive**, then
checks positive quantity and duration against the current host maximum, primitive
shape and supported medium/delivery/bundle flags. An M1 Heal plan with a foreign
key cannot dispatch by enum. A known key whose primitive changes also refuses.
Current hot-replaced quantity, duration or capability limits are rechecked before
release and again at delayed activation. Existing accepted active slows retain
their settled expiry; schema replacement is not retroactive rollback.

All gameplay mutations use finite scratch state. Release stages complete writes,
resource debit, accepted action ID and queue insertion. Advance stages every due
action and expiry up to the requested tick. An immutable owned Observation with
before/after world and accepted action events allocates **before** nonthrowing
state adoption; static assertions cover state/result moves. Actor edits use fixed
value copies; catalog replacement validates and copies bounded strings before
nonthrowing catalog/world adoption. Standard allocation exceptions propagate with
the old host/prepared input intact. Allocation-failure injection is not executed;
this guarantee is established by source ordering and noexcept assertions, not an
OOM experiment. No arbitrary external callback transaction is claimed.

### Time, resource and overlap laws

The caster pays exactly **10 mana per effect on accepted release**: 10 for S1/S3,
20 for S2/S4/mixed healing+slow. Failed prepare/release pays nothing. Delayed
release reserves one queue slot and accepts the prepared candidate point as a
fixed world-mm G0 aim. Changing the request, camera authoring context or caster
position afterward cannot move this point or the already normalized radius.
Activation queries current eligible, living actor centres, including the exact
radius boundary, and checks current caster lifetime/permission/schema. No second
mana debit or funds reservation occurs; a cast paid from exactly 20 mana executes
with zero mana remaining.

Contact is immediate and singular: the explicit actor must still be living,
eligible, willing and within inclusive **50 mm** centre distance of the caster.
There is no fallback. Burst supports the M1 inclusive disk query in G0; actor
willingness is a contact predicate, while disk membership uses the host's eligible
flag. The reference policy has no faction, line-of-sight, knowledge, resistance,
physical contact manifold or production combat permission system.

S2/S4 due time is release+**100 ticks**, each 10 ms. Slow is active from activation
through the tick before activation+**200 ticks**: [100,300). S3 uses [0,200).
`advance_to` accepts nondecreasing bounded simulation ticks; no wall time is read.
It visits finite event boundaries, expires statuses first, then activates casts
by ascending accepted action ID at each due tick. Distinct accepted actions use
this declared chronological/ID order; an individual together bundle is never
converted into sequential authored `then`.

Every binding resolves from one pre-activation snapshot before writes. Selected
(binding,actor) pairs retain separate scope even when both names select the same
actor. Two writes to that actor's health field (heal/heal, fire/fire or heal/fire)
or speed field in one activation refuse as `Overlap`, independent of binding name,
effect order and actor storage order. A speed write encountering an existing slow
refuses the entire activation as `ActiveSlow`; there is no stacking, replacement
or refresh. Expiry occurs before a new activation at the same tick, allowing a new
independent status then. Base speed stays separate and the active multiplier is
exact permille; effective speed is represented in thousandths of mm/s with no
rounding or movement integration.

Health clamps to [0,max_health]. Eligibility is fixed before the whole activation;
a selected actor can reach zero health from fire and still receive that same
bundle's finite slow. Future queries exclude zero-health actors. These are explicit
fixture laws, not implied death/resurrection or balance mechanics.

Any due failure rejects the **complete advance**: original clock, active statuses,
health/resources, action counter and every pending slot survive, including earlier
successful scratch actions in that attempted step. Failure records the attempted
event tick and accepted action context. A failed release allocates no action ID
(error action=0); successful logical IDs are host-local and never reused. Nothing
automatically retries a refusal. The owner must explicitly correct the cause or
cancel the pending action. Cancellation frees its slot/plan handle and does not
refund accepted release mana; a repeated cancellation refuses. A poisoned queue
can therefore block advancement until the owner acts. This tradeoff is deliberate.
There are no ambiguous partial external effects because this host has none.

### Independent literal runtime baseline and actual caller

Contract checks compare full before/release/onset/expiry snapshots against authored
values. Only the queued plan's byte accounting is taken from its compiler metric;
world mechanics are independent literals. The common starting actors are caster
C=(225,0), health100/mana100/ineligible/can_cast; T=(250,0), health50/eligible;
O=(251,0), health80/eligible. Maximum health100, base speed1000 mm/s. G0 aim=(0,0).

| Fixture | Release state | Onset state | Expiry boundary |
| --- | --- | --- | --- |
| S1 | tick0 C mana90, T health62; no queue/status | immediate | no status |
| S2 and S4 | tick0 C mana80, T health50, due100; tick99 unchanged health/speed | tick100 T health42, factor750 until300; O health80 unaffected | tick299 factor750; tick300 factor1000, T health42 |
| S3 | tick0 C mana90, T health50, factor750 until200; O unaffected | immediate | tick200 factor1000, T health50 |
| Novel heal+slow | tick0 C mana80, due100 | tick100 T health62, factor750 until300 | ordinary shared status expiry |
| S5 | plan inspection remains exact | runtime UnsupportedEntry, no mutation | no installed charge/event |

The actual `magic_reference_demo` additionally moves T to (1000,1000) after release
and moves O to (150,200), exactly 250 mm from the captured aim. At tick100 T remains
health50/factor1000; O changes health80->72 for S2/S4 or80->92 for the novel healing
bundle and gets factor750 until300. Output is read from adopted snapshots and
asserted, not a transcript of assumed execution. An independent caller scenario
also changes the camera/scale context and request after preparation; the accepted
point and 250 mm world radius remain fixed.

### Actual resource and arithmetic model

All HostLimits can tighten fixed ceilings, including zero; expanding any of seven
size quotas or five uint64 counter horizons refuses. Identity/time counters check
exhaustion before addition. Positions are validated before int64 subtraction;
squared unsigned distance multiplication/addition is checked before the inclusive
comparison. Health addition uses a bounded uint64 intermediate; no allocation size
comes from an authored coordinate, duration or quantity.

| Resource | Bound and evidence |
| --- | --- |
| Actors | 16 fixed slots; lower quota enforced, including zero. Each generation <=1,000,000. First-free exhausted generation refuses. |
| Pending casts | 8 fixed slots. Eight accepted releases then a ninth refusal preserves queue/funds; cancellation permits a fresh monotonic action in the freed slot. Pending=0 still allows immediate contact. |
| Active statuses | 16 fixed actor-local slots, one slow per actor. Exact 16 accepted;15/0 refuse. Capacity is checked at activation, with no future reservation promise. |
| Targets | <=128 (binding,actor) selections across a complete operation. Eight casts selecting16 actors exactly reach 128;127 refuses the entire advance. |
| Selector work | <=1024 visited slots across the complete operation, including empty slots. Eight casts x8 bindings x16 slots exactly reach1024 with only 64 selected pairs;1023 refuses without adoption. |
| Complete work | <=16,384 charged units. Fixed80 setup/copy/report units plus variable schema/name checks, selector visits, selected pairs, field scans and event scans. This is a deterministic operation model with fixed-width helper costs, not CPU cycles or a timing guarantee. Exact S2 activation work308 accepts,307 refuses and preserves paid queue/world. |
| Runtime plan retention | One prepared plus up to8 pending handles, each M1 logical bytes <=32,768, at most288 KiB distinct logical plan payload charged conservatively; no deep runtime copies. Exact per-plan metric accepts and one less refuses. M1's metric excludes unused capacity/control blocks/allocator overhead. |
| Clock/revision | Clock0..1,000,000,000 ticks; revision0..1,000,000,000. Nondecreasing advance; expiry/due arithmetic checked. A fire-only cast due exactly at the horizon succeeds without adding onset twice. |
| Action/ticket IDs | Monotonic host-local values1..1,000,000; zero not an accepted identity. Tight horizons prove action/ticket/revision exhaustion with no debit/queue mutation or ID reuse. |
| Actor scalar fields | Coordinates +/-1,000,000 mm; positive max health <=1,000,000; health within max; mana/base mm/s speed0..1,000,000. Contact <=50 mm; compiler's radius <=1,000,000 mm. |
| Retained state/outputs | Fixed arrays, bounded catalog of at most3 schemas and64-byte keys; one host marker. This ABI reports sizeof host 1872, Snapshot 1496, Observation 3216 bytes, excluding separately allocated catalog/marker/control blocks and retained plans. Each report allocates one bounded object/control block before adoption; caller-retained reports have no host-owned history cap. No exact heap/RSS guarantee is claimed. |

### Commands, results and saved logs

Current build reuse is inspected before changes: `projects/magic/build`, Debug,
GNU16.2.1, C++23, MAGIC_SANITIZERS=ON. All configure/build/native checks use the
workspace wrapper, one job, clone-local incremental output; complete stdout/stderr,
exact command and immediate `exit_status` are saved. No native lock cancellation,
resource refusal, allowance/auth workaround, dependency retrieval/install or sibling
cache/process/ref write occurs. Existing LSan tracing limitation is preserved;
ASAN_OPTIONS=detect_leaks=0 is command-local, UBSAN_OPTIONS=halt_on_error=1 remains.
No leak acceptance is inferred. Existing compiler/M1 source, fixtures, checks and
prior logs remain unchanged.

| Actual wrapper command (from clone root) | Complete evidence, result |
| --- | --- |
| `/mnt/projects/cheryl-features/scripts/build -- cmake -S projects/magic -B projects/magic/build -DCMAKE_BUILD_TYPE=Debug` | `magic/logs/M2/configure-01.log`, exit0. |
| `/mnt/projects/cheryl-features/scripts/build -- cmake --build projects/magic/build --parallel 1` | `magic/logs/M2/build-01.log`, `build-02.log`, `build-03.log`, all exit0. Later builds add the three activation checks, header contract/include clarification and actual S5 inspection output. |
| `/mnt/projects/cheryl-features/scripts/build -- cmake --build projects/magic/build --target magic_host_checks --parallel 1` | `magic/logs/M2/build-04.log`, exit0; strengthens the overlap witness to create the same logical actors in both actual insertion orders. |
| `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir projects/magic/build --verbose --parallel 1` | `magic/logs/M2/accumulated-01.log`, exit0, **11/11 entries**. Runs the entire prior 9-entry M1 suite exactly once plus initial 34 host cases and 6 reference caller scenarios. |
| Same environment and wrapper CTest command with `-R '^magic_(host_contracts\|reference_caller)$'` | `magic/logs/M2/affected-01.log`, exit0, **2/2 affected entries**. Final **37 host cases** and 6 caller scenarios after source/test changes; no repeat of unaffected M1 entries. The saved log contains the exact shell-quoted regex (the table escapes its pipe for Markdown). |

Same environment/wrapper CTest command with `-R '^magic_host_contracts$'` is
saved in `magic/logs/M2/affected-02.log`: exit0, **1/1 affected entry**, all 37 host
cases pass after the insertion-order witness is strengthened. Production host code
and the already-checked reference caller do not change in this final test edit.

Final unique suite remains 11 CTest names: prior 9 M1 names (35 compiler cases,
4 ordinary callers, 6 file fixtures, 9 CLI processes), one host entry with 37 final
cases, and one reference caller entry with 6 scenarios (five spoken executions,
one S5 inspection/refusal). The focused runs recheck existing names (two entries, then one); they
do not increase the unique count. All executed builds and native checks pass; no sanitizer
findings appear. Format, evidence/source hashes and staged review are saved in
`magic/logs/M2/static-review.log` and `staged-review.log` during finalization.

Executed host coverage includes independent full literals, amount/clamp/eligibility,
contact50, current moved/departed/replaced targets, accepted aim/context lifetime,
plan lifetime, immutable old observations, forged/foreign/superseded/consumed and
stale authorization, live permission/willingness/mana/contact, key/primitive/hot
schema/capability refusals, active-status rules at prepare and activation,
heal/fire and speed overlap in both actor/effect orders, complete-step rollback,
explicit cancellation, empty selections, actor/queue/status/selector/target/work/
plan/time/ID horizons, invalid owner inputs and unsupported ray/engraved/frame.

## M2 handoff and remaining boundaries

M2 is ready for coordinator review as one finite spoken reference host. S1–S4 and
novel healing+slow produce actual reference-state changes. M1 accepted compiler
coverage remains intact. This unit does not implement item charges, authenticated
trigger dispatch, replay/dedupe, checkpoint restoration, durable saves or external
exactly-once delivery. S5/ray/item-event are runtime-refused; only release-mode
spoken contact and immediate/delayed G0 disks execute. No general VM, physics,
Engine combat integration, line-of-sight, graphical spellbook, learner progression,
production mana/balance system or fun/comprehension playtest is established.
GNU/Linux Debug ASan/UBSan is verified; leak checking, allocation fault injection,
other platforms/ABIs/toolchains and Engine SDK integration remain unverified.

The finite transactional scratch owner is the scope growth that strengthens this
proof: it makes live overlap, failed scheduling and resource accounting observable
without callback rollback claims. Its fixed sizes and private API keep it
reversible. Next evidence is coordinator review and a separately assigned bounded
item/event/charge/checkpoint unit if selected; that unit must define event authority,
deduplication retention and coherent armed/spent restore without claiming durable
production delivery. No such work begins here. Worker ends idle after its local
checkpoint; coordinator publisher handles ordinary feat/magic backup.

## MAGIC-M3 — proposed caller and decisions before API growth

Accepted predecessor inspected: clean feat/magic at 8b6dcad36002eaaf9007b94b712bb2cf2288a64a; M1/M2 receipts, actual compiler/host and accumulated/affected logs agree with the coordinator's counts. M2 remains spoken-only through prepare/release; this unit adds a distinct native-owner item entry. No intentional v3 taste inference is added: earlier intentional author/commit provenance remains the evidence; M1/M2 interfaces establish compatibility only. Runner admission records configured ultra for this exact task.

Proposed small caller (not yet executed):

```cpp
plan = compile(healing_engraving, schema);             // actual owned M1 plan
host = ReferenceHost::create(schema, limits);
item = host->install(plan, funding_actor);            // trusted fixture owner; prepay
armed = host->checkpoint();
host.reset();                                       // no compiler/catalog/source lifetime dependency
restored = ReferenceHost::create(schema, limits);
restored->restore(armed);                            // validates; fresh live incarnation
event = restored->admit_touch(17, item, toucher);     // owner admits a logical native fact
result = restored->touch(event);                     // shared payload, heal + charge + receipt together
spent = restored->checkpoint();
restored->restore(spent);
restored->touch(restored->admit_touch(18, item, toucher)); // explicit Spent refusal
```

**M3-019 (selected, reversible private contract).** Installation is an explicit trusted native-owner operation, using a living can_cast funding actor's current mana and current key+primitive/quantity/capability schema. It accepts only immediate one-charge item-touch plans with event-toucher bindings, pays 10 mana/effect at installation, and retains the actual immutable plan. A successful touch spends the prepaid charge; failed installation/application, disable/remove, and cancellation never refund. Items have first-free slot/generation identities, enabled state, and a per-installation plan; spent stays visible and inert. No physical item/contact simulation is claimed: owner admission asserts the touch fact. Spoken/item payload resolution and overlap/status/write logic are shared, with an explicit event recipient rather than a fake caster. Confidence high in ownership/refusal, medium in fixture pricing/API.

**M3-020 (selected).** At most eight items and sixteen lifetime touch receipts, caller-tightenable. Native-owner admit_touch accepts a nonzero bounded logical fact ID plus item/toucher references, checks duplicates, and returns an opaque incarnation-bound capability with a checked monotonic nonce. An ID alone cannot apply effects. Each admitted fact remains in the ledger forever within this timeline, whether outstanding or consumed; no arbitrary eviction or TTL. Failed application preserves the entire world/items/ledger and leaves admission usable after explicit correction; successful application consumes it atomically with payload/charge. Default, foreign, consumed and removed/replaced-item inputs refuse. Copies share one use. Restore revokes all old live inputs; the trusted owner may resume an outstanding logical fact with resume_touch, which issues a capability only for that existing unconsumed ledger entry. Duplicate admission always refuses. Fact/nonce horizons and fixed ledger capacity refuse explicitly without wrap. This is native owner authority, not network authentication. Confidence high; lifetime ledger exhaustion is an intentional finite limit.

**M3-021 (selected).** Checkpoint is a separate opaque immutable owned value, never Snapshot. It retains complete finite state, actual pending/item plan handles, schema/limit identity, ledger and issuance counters; it excludes live prepared casting and event capabilities. Capture and restore validate bounded state, canonical slots/generations, references, times/statuses, exact schema/limit compatibility and retained payloads before allocation/adoption. Restore into an existing compatible owner replaces the whole timeline with a fresh incarnation, clearing prepared input; failed restore preserves that owner and previous outputs. Whole-world rewind intentionally restores charges/health/clock/ledger to that earlier world; duplicate delivery is prevented within each resulting timeline, not across deliberate rewinds. No disk/process/crash/external exactly-once guarantee. Confidence high in narrow in-memory design; no allocator fault injection promised.

Plan limit accounts per handle payload (existing M2), fixed arrays bound owner/checkpoint metadata, and checkpoint validation meters complete scans/key comparisons. Caller-retained immutable checkpoints/observations are outside host history quotas. Allocation exceptions propagate before adoption; no arbitrary callback participates. Pending actions may retain currently departed actors as cancellation-only references, but restore will require live referenced actors so such a checkpoint explicitly refuses until corrected/cancelled; no implicit repair. Source/schema edits can likewise require correction/cancellation before capture. These narrow choices strengthen coherent continuation without building a general event bus or save format. Next evidence: literal armed/spent worlds, real owner destruction/restore and delayed boundary continuation, then focused refusal/budget cases plus accumulated regression once.

## MAGIC-M3 implemented contracts and evidence boundary

M3-019–021 are implemented with the proposed ownership boundaries. New ordinary caller is `projects/magic/src/engraving-demo.cpp`, separate from test helpers. It compiles an invented Mend healing clause, prepays an item, destroys its owner/compiler input, restores armed, applies an admitted native touch (+12hp, charge1->0), destroys/restores spent and observes SpentItem with no extra healing. Its delayed example retains an actual compiled plan, nonzero world aim(1000,-500), inclusive radius250mm, due100 and slow expiry300, destroying/restoring both pending and active owners. It advances explicitly at99/100/299/300 and verifies no second activation. These are executed native assertions when the saved caller log passes; no callback or source regeneration supplies continuation.

`Reference::Snapshot` remains the same observational type and cannot convert to Checkpoint. New ItemSnapshot is also observational and exposes item/charge/receipt facts, never a way to forge event authority. Checkpoint has only immutable opaque shared data, no mutable state constructor. Each capture retains up to eight pending plus eight inscription plan handles and one bounded catalog copy, counters/limits and fixed arrays. No text or plan deep copy occurs at runtime. A host may additionally retain one prepared plan: at most17 handle payloads ×32768 logical bytes =557056 logical payload bytes before handle sharing; each checkpoint retains at most16 ×32768 =524288. Allocator/control-block overhead and caller-retained checkpoint/report history are outside quotas. All are finite, not exact RSS promises. State copy overhead is fixed in the operation model; capture/restore additionally charge220 units for bounded schema metadata/keys before validation/copy, and charge every state scan, ledger uniqueness comparison and retained plan check.

Installation authority is the trusted single-thread native owner API plus a living can_cast funder with sufficient mana. It does not require a spoken target or use prepare/release. The charge is prepaid (10/effect) at successful installation, even if the eligible recipient is already at maximum health; a later successful clamped activation still consumes its single charge. The funder may subsequently depart; the item payload has no fake caster or ongoing mana dependency. Trusted admission asserts contact; no item position/physics or network authentication is modeled. Enabled=false refuses application without consuming admission; reenable can make that same unconsumed capability usable. Removal retains the generation counter and ledger, frees only the inscription handle, and never refunds. First-free exhausted generation refuses rather than skipping it.

Fact IDs are supplied only at the trusted owner admission boundary (range1..max_fact_id). They are not arguments to touch; default/copied observations/IDs cannot create TouchEvent. Admission assigns nonce1..max_event_nonce and appends one of16 fixed lifetime receipts (outstanding and consumed both count). There is no time TTL or eviction; issuance horizon is count/ID bounded. Admission itself changes ledger/revision but does not heal/spend. Failed application preserves that admitted state, charge and ledger bit; no automatic retry. Successful application resolves all bindings before writes and stages the entire state and owned ItemObservation before nonthrowing adoption. Copying or owner resume of a capability cannot produce a second success. resume_touch is an explicit trusted reauthorization of an existing unconsumed fact, not arbitrary raw-ID execution; old live inputs remain foreign after restore.

Checkpoint capture and restore both validate: clock/revision/action/ticket/event horizons; canonical slots, scalar ranges and generation counts; status bounds and expiry; pending action uniqueness/references/due ticks/aim, real retained plan and current spoken capability/schema; item charge/prepayment/plan identity and current engraved capability/schema; ledger nonce order, unique fact IDs, bounded historical references. Historical departed/replaced item/actor references remain valid replay tombstones, not usable live targets. Pending work with departed/unpermitted caster or a changed unsupported schema refuses capture until owner correction/cancel; pending effect durations beyond max_tick also refuse. Existing active slow is committed state that only expires, not an effect redispatch. Exact catalog ordering/keys/schemas/flags and all HostLimits must match the destination. Restore stages a new owner marker and state before adoption, restores logical counters but clears prepared input, and invalidates every old casting/event capability. Failure preserves current host, checkpoint and old outputs. Checkpoint does not retain live tickets/owner markers. Allocation exceptions still propagate before adoption; arbitrary failure injection is unexecuted.

Whole-world restoration is an intentional rewind: restoring an armed checkpoint can legitimately admit the same fact again in a new timeline after health/charge/ledger have also rewound. Restoring spent preserves its receipt and inert charge. This proves no duplicate inside one restored timeline, not delivery across intentional rewinds, processes, disks, crashes or external systems. Catalog/limit migration and cyclic/general triggers are deliberately unsupported.

### M3 actual checks and handoff

Configured/run in the existing clone-local `projects/magic/build` Debug C++23 ASan+UBSan tree, GNU16 toolchain, warnings as errors, one job. Every configure/build/native CTest command goes through `/mnt/projects/cheryl-features/scripts/build --`. Full stdout/stderr, exact command and immediate exit are saved under `feature-development/magic/logs/M3/`; all seven native invocations exit0. Build02 waits normally for the shared lock in its original tool session; no cancel/relaunch or resource-refusal retry. No dependencies, pins, siblings, archives, remotes or shared caches change. Existing no-swap/adaptive/floor policies apply. Worker run20261024T051119Z-e3455591c7fa, configured ultra, accepted base8b6dcad.

| Log | Actual command after wrapper (environment on native CTest below) | Result |
| --- | --- | --- |
| configure-01.log | `cmake -S projects/magic -B projects/magic/build -DCMAKE_BUILD_TYPE=Debug -DMAGIC_SANITIZERS=ON` | exit0, existing incremental configuration |
| build-01.log | `cmake --build projects/magic/build --parallel 1` | exit0, all nine executable/library targets built (seven executables, two libraries) |
| accumulated-01.log | `ctest --test-dir projects/magic/build --verbose --parallel 1` | exit0,13/13 entries once: all9 M1 entries,2 M2 entries,2 M3 entries; initial29 M3 contract cases and3 new caller scenarios |
| build-02.log | `cmake --build projects/magic/build --target magic_engraving_checks magic_engraving_demo --parallel 1` | exit0 after checkpoint-only horizon/copy-meter review changes and combined-state coverage |
| affected-01.log | `ctest --test-dir projects/magic/build --verbose --parallel 1 -R '^magic_engraving_(contracts\|caller)$'` (actual regex has unescaped `|`, see saved command) | exit0,2/2,32 M3 cases and3 callers |
| build-03.log | same two affected build targets | exit0; caller/test-only nonzero-aim and restored-exhaustion witnesses |
| affected-02.log | same two affected CTest names | exit0,2/2, final34 M3 cases and3 callers; no sanitizer findings |

All CTest commands set only command-local `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`, preserving address/undefined checks and the known tracing-related leak-check gap. No new full regression rerun follows passing accumulated checks: later production changes are inside new checkpoint validation only, and final changes affect only new tests/caller. M1 retains35 compiler cases,4 ordinary callers,6 file fixtures and9 CLI process scenarios; M2 retains37 spoken-host cases and6 reference callers (five spoken executions and one engraved spoken-entry refusal). These counts are not extra CTest entries. New total is13 unique CTest entries, not17 due to focused reruns.

Independent literal before/admission/touch/restored armed/spent worlds assert health50->62, prepaid mana100->90, charge1->0 and receipt false->true. Tests also cover exact target identity/current willingness/eligibility/liveness; live unknown/changed key, primitive, quantity and entry capabilities; default/foreign/copied/repeated capabilities; disabled/spent/removal/reuse; actor/item generation references; exact8 items,16 retained receipts,1/0 nonce horizon and bounded fact IDs; exact/insufficient10/9 funding; per-plan bytes exact/one-short; touch exact168 work vs167, selected/selector1 vs0; capture/restore fixed base-state exact348 work vs347 capture refusal; revision/time horizons; coherent combined items/receipts/pending/slow; owner/source/catalog destruction; old report immutability; fresh restore authority and explicit outstanding resume; incompatible/default checkpoint refusal and continued prior casting/checkpoint usability. Nonzero accepted aim(1000,-500) survives caster movement, owner destruction and restore, with inclusive boundary(1250,-500) included. Restored nonce/generation exhaustion never resets or wraps. Final native assertions are34 cases; individual loop variations are not counted again.

M2's actual full Snapshot literals and overlap-order checks remain unchanged. M3 adds ItemSnapshot literals; source-dependent `plan_bytes` comes from the already validated input plan metric, while health/resource/charge/timing/identity values are independent literals. Opaque Checkpoint has no caller-editable state constructor; invalid-state checks are exercised through invalid capture and incompatible/default restore, not a forged deserialization API. The restored active status is data with a future expiry; it never redispatches old effects.

Static/staged review is saved separately. No native failures occurred. Pending boundaries remain: no disk codec, process restart, crash/power-loss safety, remote/event authentication, external exactly-once guarantee, automatic rearming, general trigger graph/bus, item physics, ray/non-G0 runtime, production Engine integration, balance/fun/graphical/learner acceptance, LSan under tracing, Windows/other toolchains/ABIs, allocator fault injection or exact heap accounting. Native-owner allocation exceptions are source-reviewed for before-adoption preservation, not OOM-tested. Catalog/limits must match exactly on restore; no migration is selected. Checkpoints/reports retained by callers can accumulate outside host quotas.

Next action: finish this single coherent M3 checkpoint and remain idle. The finite controller alone may admit pinned MAGIC-M4 after this committed passing receipt, clean/drained predecessor and saved evidence gate; M4 must inspect real M3 source/receipt/logs before depending on it. No M4/M5 work or scheduling starts here. Publisher owns ordinary feat/magic backup transport.

## MAGIC-M4 — proposed caller and selected contracts before APIs

Predecessor gate inspected at clean9321bedd7d7543c1b7e96b1459d02122edfa2abb: committed M3 receipt matches feature-round-20261024-0500/MAGIC-M3, ready/true, and all seven committed nonempty native logs end exit0. Actual source retains shared key+primitive validation/payload writes, opaque item authority, non-evicting receipt ledger and distinct coherent checkpoint with fresh incarnation. Saved evidence confirms13 accumulated entries, final34 M3 cases/3 callers plus retained M1/M2 counts. No prerequisite contradiction. Runner20261024T052652Z-325fb79d80d5 records configured ultra. Existing intentional v3 provenance remains above; recent APIs establish compatibility only, with no new taste inference.

Proposed ordinary caller (names provisional, not yet executed):

```cpp
book = Spellbook::create(limits);
book.add("healing", "A small restoration");
book.edit("healing", mend_contact, notes, catalog, learner); // accepted revision1
lesson = make_lesson(bilingual_fire_slow, exact_gloss, registered_cinder_slow_rule,
                     changed_practice_example, catalog, limits);
book.edit("healing", newly_composed_mend_heal_plus_cinder_slow, notes, catalog, learner);
// Compiler recognizes it, learner lacks cinder.slow@1; revision1 survives.
learner.learn(lesson); // teaches just that production, idempotent; no host token
new_entry = book.edit("healing", newly_composed_mend_heal_plus_cinder_slow, notes, catalog, learner);
print(new_entry.accepted.common());
ticket = host.prepare(new_entry.accepted.plan(), ordinary_request);
result = host.release(ticket); host.advance_to(100);
book.edit("healing", malformed_draft, notes, catalog, learner); // revision2 remains usable
```

**M4-022 (selected, reversible).** Add exact bounded compiler metadata for eight registered rules: three Common effect concepts, explicit target bridge, Mend heal/slow and Cinder fire/slow. The actual checked production records its rule per clause and an owned value mask in Plan; no text heuristics. School clauses also require the corresponding Common effect concept plus bridge. The fixture always recognizes outer Common constructors; learned syntax is a separate consumer policy. Common can replace unknown school phrasing only when its operation concept/bridge is learned, and the host still checks current key/schema/permissions. Medium confidence in private registry shape, high in separating recognition/knowledge/authority.

**M4-023 (selected).** An eight-slot private book owns identities/titles and immutable accepted revision handles, plus one current owned Attempt per slot. Accepted source/Common are owned by the retained actual Plan, annotations by the entry. Failed syntax, unsupported capability, unknown knowledge, revision/work/storage/text edits keep accepted source/plan/revision and record a bounded repairable draft/diagnostic; oversized drafts keep only the allowed prefix and original byte count. Successful edits increment a checked per-entry revision; no identity reuse/removal/history API. Old handles/queued/item/checkpoint plans remain unchanged. Caller-retained history is outside book quotas. High confidence in ownership; medium in reversible API.

**M4-024 (selected).** One immutable tome lesson owns a worked bilingual source/plan, exact canonical Common gloss, a changed practice source/plan/gloss, annotation and compiler-derived aligned clause copies. A named registered school production must occur in both actual examples; contradictory gloss, unknown rule/identity or unchanged practice meaning refuses before learning. Knowledge is an owned version1 eight-bit rule record with checked revision; learning requires known operation concept/bridge and idempotently sets only the taught bit. No reward ledger, provider or host authority is added. High confidence in deterministic checks; human teaching/enjoyment are hypotheses.

Limits selected before input: eight book entries, one lesson per immutable tome object, eight registered rules and at most four aligned effect clauses. IDs are1..32 ASCII identifier bytes, title<=64 and annotation<=256; each authored/gloss/draft text<=8192 bytes. Learning limits may tighten entries/text/revision, work<=262144 and retained logical bytes<=524288. Reserve12288 work per operation and12288 retained bytes per configured book slot (one lesson envelope for tomes) for bounded draft/diagnostic/metadata copies, including failure publication; work cannot tighten below that reserve. Further compile work is admitted through M1's remaining per-call work budget (<=65536), with actual successful metrics charged; retained actual plan payloads and aligned text copies are added with checked arithmetic. Fixed envelopes conservatively cover strings/metadata, not allocator overhead/RSS. Limits/refusals happen before variable allocations; allocation exceptions propagate before pointer adoption. Failure publication uses its reserved bounded envelope even when the payload budget is exhausted. This choice keeps over-budget attempts inspectable without unbounded failure history. Next evidence: exact literal meanings/Common, actual lesson transfer/cast, all selected budget boundaries, ownership and predecessor regressions.

M4-024 refinement before checks: the learner retains at most four immutable lesson handles, one per teachable school production. It refuses reuse of a studied identity with different rule/annotation/source/practice, while independently owned equivalent definitions are idempotent. This small addition proves duplicate identity handling without a general lesson catalog or reward system. Four handle assignments do not deep-copy lesson text; each lesson payload remains bounded by524288 logical bytes, at most2MiB of retained lesson models per learner before sharing. Learn itself has fixed eight-bit/four-receipt work (<=four32-byte identity checks and two8192-byte definition comparisons plus bounded annotation), no variable allocation/callback. Missing concept/bridge or revision exhaustion preserves knowledge. Book compilation admits the replacement's remaining plan-byte quota through M1 before allocation; temporary staging may coexist with the prior accepted plan. Tome retains at most two plan payloads plus aligned clause copies, with at most two additional bounded32768-byte Common-provenance plans during construction. Fixed object arrays/allocator overhead are separately finite, outside the logical payload model; zero entry payload quota does not claim zero object RSS.

## MAGIC-M4 implemented contracts and exact proof

M4-022–024 are implemented. The small native caller is `projects/magic/src/learning-demo.cpp`, independent of test helpers and library internals. Authored originals and independent canonical Common readings are six files in `projects/magic/fixtures/m4/`. The Cinder/Common worked example is fire8 plus slow750 for200 ticks, radius250mm, onset100. The practice transformation changes fire to Mend healing6 and slow to500 for100 ticks, onset50. The caller edits an existing usable Mend12 contact spell into a different combination: Mend healing9 plus Cinder slow600 for300 ticks, radius300mm, onset50. This actual new text uses existing primitive/grammar constructors; the learning library contains no whole-spell dispatch. It differs from both the tome practice and earlier M1–M3 heal/slow examples.

The caller's initial learner recognizes the compiled new plan but lacks exactly `cinder.slow@1`. It owns that failed draft while accepted healing revision1 survives. The lesson prints exact aligned clauses: `on near bridge Cinder@1 { 2 s 750 permille neth } ;` equals `on near bridge Common@1 { multiply speed by 750 permille for 2000 ms } ;`. Both language name and Cinder/Mend tokens are explicitly labelled invented; Common is controlled syntax. Learning the duration-first production is idempotent and yields only that bit. The new edit accepts revision2, prints its exact Common, and still gets SchemaMismatch when the live host's slow max_quantity is599. After restoring supported schema, a malformed sequential edit reports owned UnsupportedComposition at byte285 and leaves revision2 usable. Ordinary host prepare/release pays20 mana, queues due50, observes hp50 at49 and hp59/speed600 at50, and expires speed at350 without another heal. Initial healing handle stays revision1/12hp. This demonstrates fixture behavior, not a human lesson outcome.

Compiler metadata comes from checked Authored language/primitive roles during lowering. It is a fixed owned RuleMask and one Rule enum per existing bounded Provenance record, with sizeof-based retention and one work charge/effect. No source-string search infers learned requirements. Common@1 operations double as operation-concept requirements; all plans also require the explicit bridge. Recognized school productions additionally require their own bits. Outer Common composition/geometry/entry constructors are assumed available to this fixture learner, not individually taught. Unknown school syntax may therefore be replaced with known Common; missing slow/heal/fire concept still prevents availability in the book, and current ReferenceHost capabilities/permissions/resources still decide casting. `magic_learning` links only the compiler; callers explicitly obtain reference-host authority separately. Knowledge is consumer policy, not a sandbox against arbitrary trusted native owner calls.

Entry identity/title/annotation are owned; source and Common are exposed through the retained immutable actual Plan. Accepted revision starts1; checked successful edits increment, failed attempts consume no revision. Each resolved-ID edit returns an immutable Attempt holding the current accepted handle plus either accepted source or a bounded owned failed draft, submitted byte count, truncation flag and structured Problem. Compiler failures retain owned spans/excerpt/repair text. Unknown knowledge retains an exact missing-rule mask. Oversized text keeps at most the configured source prefix; oversized annotations are refused and do not replace accepted annotations. No full unbounded rejected input/history is retained. Allocation exceptions propagate before accepted/draft pointer adoption, preserving prior outputs; this is source-reviewed, without fault injection.

One Lesson owns ID/annotation, actual worked and practice Plan handles and up to four aligned original/Common clause copies. Exact gloss equality and reparse typed meaning, occurrence of the taught registered school rule in both examples, and changed practice meaning validate mechanical teaching data. Annotation is non-executable descriptive text, not authority or an interpreted rule. Unknown version/operation concept as taught school rule, malformed input, contradictory gloss, missing rule or unchanged practice refuse. All fixed grammar names/revisions remain private and reversible; no external language-pack format is introduced.

Learner version1 holds eight known bits/revision and at most four immutable studied Lesson handles. Same studied ID with equivalent source/practice/annotation/rule AND typed meaning is idempotent; inconsistent reuse refuses DuplicateIdentity before any state change, including identical prose compiled with different symbolic keys. Already-known productions add no revision/reward/extra receipt. Prerequisite is the learned corresponding Common operation concept plus target bridge. Each new school rule advances the bounded knowledge revision once; no lesson can unlock another operation by prose or mint Ticket/TouchEvent. Copying book/knowledge/tome/checkpoint data supplies no live authority. Four retained lesson handles avoid deep definition copies; fixed bounded comparisons remain below32768 logical character/field units per learn even when typed meaning is checked.

Budget model is intentionally conservative. Book reserves12288 logical bytes for each configured slot's identity/title/accepted metadata/current failure envelope, plus current accepted Plan metrics; maximum8 slots and eight32768-byte plan payloads. One staged replacement Plan may coexist with the prior accepted plan; M1 admits its allowed replacement payload before allocation. At creation, checked division rejects an insufficient slot envelope before multiplication. Every later logical addition/subtraction is checked before the corresponding variable copy/adoption. Tome reserves12288 plus two actual plan metrics and aligned text bytes; up to two additional temporary Common-provenance plans are each bounded32768. Work reserves12288 for bounded failure/metadata copies and fixed scans, then charges actual successful M1 metrics, canonical comparisons, metadata and aligned copies. M1 compile work admission is min(65536,remaining); failed compilation exposes no exact work metric, so the whole admitted compile work is conservatively charged. These are logical payload/work models, not CPU cycles or exact heap/RSS. Fixed object arrays/control blocks/allocator bookkeeping and caller-retained immutable history are outside variable payload quotas but have finite per-owner ceilings. No new caches/dependency pins or siblings change.

Current source review adds explicit duplicate-identity comparison of typed worked/practice meanings: same source can bind another host key under a different catalog, so text equality alone is insufficient. The focused regression exercises that exact case and preserves prior knowledge. A final test-only minimum envelope/zero-text witness checks the promised inspectable failure even with no compile/text payload budget. These refinements affect only learning code/tests, leaving compiler metadata and all accepted host/item/checkpoint source laws unchanged after the accumulated run.

### M4 executed commands/counts and pending boundaries

All configure/build/native checks use the existing clone-local Debug C++23 GNU16 ASan+UBSan build through `/mnt/projects/cheryl-features/scripts/build --`, one job. Native CTest environment is command-local `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`, retaining address/undefined checks and the existing tracing-related LSan gap. Complete command/stdout/stderr/immediate exit logs live under `feature-development/magic/logs/M4/`.

| Log | Actual command after the required wrapper | Immediate result |
| --- | --- | --- |
| configure-01.log | `cmake -S projects/magic -B projects/magic/build -DCMAKE_BUILD_TYPE=Debug -DMAGIC_SANITIZERS=ON` | exit0, incremental existing configuration |
| build-01.log | `cmake --build projects/magic/build --parallel 1` | exit2: new test helper `failed` collides with the failure counter; compiler/host/learning libraries and old executables build first. This failure is preserved, not called a passing check. |
| build-02.log | `cmake --build projects/magic/build --target magic_learning_checks magic_learning_demo --parallel 1` | exit0 after test-only helper rename; both new executables build |
| accumulated-01.log | `ctest --test-dir projects/magic/build --verbose --parallel 1` | exit0,15/15 entries once, including all13 M1–M3 names; initial23 M4 contract cases and1 full teach-compose-cast caller |
| build-03.log | same two learning build targets | exit0; narrow duplicate-definition typed-key correction and regression |
| affected-01.log | `ctest --test-dir projects/magic/build --verbose --parallel 1 -R '^magic_learning_(contracts\|caller)$'` (saved command records the actual unescaped regex alternation) | exit0,2/2 entries,24 M4 cases plus1 caller |
| build-04.log | `cmake --build projects/magic/build --target magic_learning_checks --parallel 1` | exit0; test-only minimum-reserve/zero-text and overlong-rule witness |
| affected-02.log | `ctest --test-dir projects/magic/build --verbose --parallel 1 -R '^magic_learning_contracts$'` | exit0,1/1 entry, final25 M4 cases; production and caller unchanged |

Native total: one configure, four builds (three pass, one corrected test-harness failure), one accumulated suite and two affected suites. No resource/auth refusal or unchanged retry. Initial configure/build queueing retains the original sessions until admitted/completed; no cancellation/relaunch. No sanitizer finding. Unique CTest entries remain15, not18 due to reruns. Accumulated M1 counts remain35 compiler cases,4 ordinary callers,6 file fixtures and9 CLI processes; M2 remains37 host cases/6 reference callers; M3 now runs its final34 cases/3 callers in this accumulated invocation. Six new M4 authored verse/Common files are distinct from M1's six unchanged input fixtures. M4 final25 contract cases and one caller are separate counts, not extra CTest entries.

Fixture-specific exact positive/one-short thresholds: new edit work14823 vs14822 (helper slot/title/annotation included), tome work23357 vs23356, tome retained17860 vs17859 logical bytes, book reserved envelope12288 per configured slot, replacement plan bytes exact vsone-short. Entry count8/9, tightened1/2 and0/1, text8192/8193 plus tightened input length/one-over and zero text, title64/65, ID32/33, annotation256/257, work reserve12288/12287, revision1/next plus0/first, invalid registry bit256, all four learnable school productions and idempotent/conflicting identity definitions are exercised. Loop variations are not counted as separate named cases. Compiler-owned diagnostic spans and old Attempt/Entry/Plan handles outlive later edits and source/catalog/tome/book destruction. Tests check independent full Meaning and Common literals for worked/new/practice plans, exact clause alignment and actual before/release/onset/expiry health/mana/speed values. Queued casts and installed inscriptions keep their original9hp/12hp despite book edits to31hp/30hp; a real M3 checkpoint retains those original plans across all original owners' destruction and executes71hp total after restore.

Static/staged review separately verifies formatting/diffs, unchanged M1–M3 receipts/log bytes and unchanged reference host/item/checkpoint source/tests/callers. The compiler changes only owned requirement metadata, sizeof-based storage and one work unit/effect; all prior grammar/Meaning/Common and host behavior pass accumulated checks. New owner mutations are synchronous single-thread fixture operations; immutable handles support stable reads, with no mutable view API.

Material pending limits: no graphical spellbook/editor, full language pack, provider/NLP, disk book/knowledge save format, migration or integration into M3 world checkpoints. M3 checkpoints retain spell meaning, not book drafts or learned records. No human comprehension/enjoyment/playtest or production balance/security/Engine integration claim. Reference runtime remains spoken contact/G0 disks and immediate one-charge item-touch; ray/broader triggers/rearming remain unsupported. No crash/process/durable external exactly-once guarantee; deliberate whole-world rewind keeps M3's timeline semantics. Other platforms/toolchains/ABIs, leak checking under tracing, allocation fault injection/OOM and exact heap/RSS/CPU accounting remain unverified. Caller-retained immutable history is outside owner quotas; source-reviewed before-adoption exception preservation is not injected-failure evidence.

Next: commit this one coherent MAGIC-M4 checkpoint and remain idle. The finite controller alone may admit pinned MAGIC-M5 after clean/drained committed passing receipt/evidence. M5 must inspect this actual lesson-to-new-cast source, typed-key correction and saved passing logs before extending annular composition. No M5 implementation/scheduling begins here. Publisher owns ordinary feat/magic transport.

## MAGIC-M5 — proposed caller, normalized baseline and decisions before APIs

Start clean79dfd22d76b020e95f524865f27771e873d909b6 on feat/magic. Actual M4 compiler requirement/book/tome/caller source and M3 reference host/checkpoint source are inspected. Both committed batch receipts match feature-round-20261024-0500 and exact milestone, ready/true; their fourteen total passing native evidence files are nonempty, byte-equal to HEAD and exit0. M4 evidence includes15 accumulated entries, final25 learning cases and a real lesson-to-new-cast caller; its one corrected test-harness build failure stays preserved. M3's shared payload, opaque authority/ledger and distinct checkpoint laws remain real prerequisites. Runner20261024T054803Z-225c8ce4ecbb records configured ultra. Workspace policy/brief/design/finite-round instructions apply; no new v3 taste inference is made. Earlier intentional provenance remains above, recent APIs are compatibility evidence.

Proposed ordinary caller (not yet executed):

```cpp
book.edit("burst", accepted_disk_with_Cinder_bridges, notes, catalog, learner);
lesson = make_lesson({"protected-centre", "target.annulus@1", invented_label,
                      annular_original, exact_Common, changed_annular_practice, exact_practice_Common}, catalog);
learner.learn(lesson);                         // learns the targeting modifier, no host permission
accepted = book.edit("burst", edited_annular_burst, notes, catalog, learner);
print(accepted.common());
ticket = host.prepare(accepted.plan(), {caster, {}, Point{1000,-500}});
host.release(ticket);                         // retain aim/shape/plan, pay ordinary20 mana
// Owner moves actors over both boundaries, removes one and changes eligibility.
checkpoint = host.checkpoint(); host.reset(); // destroy authoring inputs as well
restored.restore(checkpoint);
restored.advance_to(100);                      // current ring members receive shared fire+slow
```

Exact selected Common target clause:
`bind near = eligible actors in G0 annulus excluding inner disk 100 mm inclusive within outer disk 250 mm inclusive around aim captured at release queried at activation ;`
It is an explicit school-compatible outer target bridge: unchanged `on near bridge Cinder@1 { 8 dp hara }` and `on near bridge Cinder@1 { 2 s 750 permille neth }` share that one resolved binding. Cinder/Mend names/tokens remain invented. This selects the assignment's explicit bridge option rather than adding an alias-only shape vocabulary. Common renders both radii and precise boundaries, captures aim at release and queries eligible living actor centres at activation. Source is controlled grammar, no natural-language inference.

Independent normalized baseline, before implementation: query EligibleAnnulus (new tag), outer250mm, inner100mm, G0, inclusive outer, excluded inclusive inner disk; inner100 < distance <= outer250. At aim0: centre(0,0) and(100,0) unchanged; (101,0) and(150,200) selected; (251,0) unchanged. Initial target hp50/speed1000: selected fire8 produces hp42 and slow750 until activation+200; excluded targets stay hp50/speed1000. Onset100 ticks; caster mana100->80 at release, no second payment. Zero inner excludes exact centre; the old disk tag with default inner0 includes centre. Practice target changes inner50/outer300, preserving payload. No new effect handler/demonstration-name dispatch is introduced.

**M5-025 (selected, reversible).** Add one appended Query::EligibleAnnulus tag and one bounded inner_radius_mm field at Binding's end; radius_mm stays outer and old aggregate/default disk semantics stay intact. Controlled Common parses/renders the exact clause above, refuses bad dimensions/ranges/boundary scope and normalizes each radius independently through existing contextual px rules. Own bounded declaration spans/production IDs for bindings (<=8) alongside existing clause provenance. High confidence in tagged semantics/ownership; medium in verbose private spelling. No general set graph, VM or effect family.

**M5-026 (selected).** Add explicit Catalog annular capability, included in compiler/host validation and checkpoint schema identity. Host accepts only known disk/annular query tags and correct normalized bounds; no fallback. Annular membership uses checked squared distance once and two checked squared radii. Charge two additional radius-work units per binding plus two selector units per actor slot (disk stays one). Existing actor16/pending8/target128/selector1024/work16384 caps remain unchanged, so elaborate annular steps can explicitly exhaust earlier. Resolve all bindings before shared overlap/status/write policy; full failed step preserves world/queue/resources. Checkpoints retain actual query/aim/timing and validate through the current capability before adoption. High confidence in finite arithmetic/refusal; no cross-version/disk guarantee.

**M5-027 (selected).** Add one ninth registered requirement `target.annulus@1`, taught by the existing bounded tome flow with target-declaration alignments derived from checked spans. At most eight target rows plus four old effect rows, all text/work/storage charged within existing ceilings. Learner retains at most five lesson handles (four school productions plus the new modifier); annular learning requires target-bridge knowledge and unlocks no effect/host authority. This is an explicit fixed registry/receipt-count extension for the one selected constructor, not a quota increase to hide selector failures. Existing tests for the old eight-bit/four-school subset are adapted to assert that subset and reject the next genuinely unknown bit. High confidence in separation; human comprehension/enjoyment untested. Next evidence: literal geometry, both storage orders, delayed crossings and retained-checkpoint caller, refusal and exact budget checks, accumulated suite once.

## MAGIC-M5 implemented behavior and evidence

M5-025–027 are implemented. New ordinary caller `projects/magic/src/annular-demo.cpp` is independent of test/library helpers. It starts with a real accepted S2 disk in an owned book, records recognized-but-unlearned annular syntax, studies the validated targeting lesson, and accepts revision2 with Cinder fire8 plus slow750 for200 ticks over inner100/outer250mm. The lesson's worked example uses Mend heal9 plus Cinder slow, with practice inner50/outer300; transfer to fire+slow uses existing generic constructors. Six independent authored source/Common files live in `fixtures/m5/`. The target declaration is language-independent and explicitly bridges both invented school clauses; aligned effect rows show the actual Mend/Cinder versus Common productions. No whole-spell/example-name logic or new effect handler exists.

The real caller releases at aim(1000,-500), pays20 once and queues due100. Before activation, one actor moves from inner100 to101, another101->100, another outside251->outer(150,200); one leaves and another becomes ineligible. It checkpoints at99, destroys host/book/learner/tome/source inputs, restores a fresh owner and rejects its old live ticket. At100, protected centre stays hp50/no slow; current ring entrants become hp42/slow750 until300; excluded inner-boundary/departed/ineligible members remain unchanged. At300 slow expires without another fire hit. It also edits the book back to an ordinary disk after capture and verifies the retained old disk entry remains a disk. These are executed assertions in the saved caller log, not assumed transcripts. Camera acquisition is not implemented: separate native tests change authoring PixelContext and caller Request after release, proving frozen normalized radii and accepted world aim cannot be moved through those old inputs.

The new Query tag is appended; old enum values and positional Binding fields remain intact, with inner_radius_mm appended/default0. Ordinary disk centre is included even with this new default; only the annular tag excludes the inclusive inner disk, including exact centre when inner0. Annular upper radius uses the existing positive<=1e6mm domain; inner is exact>=0 and strictly smaller. Compiler parses no recursive shape expression. It rejects unknown shapes, misplaced/ambiguous exclusion, false boundary keywords, equal/reversed/negative/inexact/wrong-dimension/unsupported-unit/overflow quantities with owned source diagnostics. Both radii can normalize through an explicit supported pixel context; no implicit ratio is introduced. Common reparses equal full Meaning, preserving timing/order/effect roles and one shared binding.

BindingProvenance owns at most eight numeric declaration spans and registered rule enums, anchored in Plan-owned source. Logical retention charges sizeof(Plan), Binding and BindingProvenance before vector allocation; metadata lowering charges one work unit/binding. Target alignment derives only from those checked spans and the exact recompiled Common gloss. Taught annular lessons add at most eight target rows to four effect rows, with copied text/work accounted before allocation. Learning adds a ninth requirement bit/fifth retained lesson handle, requires target-bridge knowledge and never adds effect concepts/permissions/tickets. Unknown bit512 refuses. Existing M4 tests preserve their original eight-rule/four-school subset assertions and now reject the actual next unknown bit. This necessary fixed registry extension is explicit; compiler/host/learning work/storage/source/effect/entry ceilings are not raised.

Current Catalog annular capability is checked at compile, live prepare/activation and checkpoint continuation; exact schema identity includes it. Unsupported frames/queries or changed/unknown effect schemas cannot become disks or dispatch by enum alone. Opaque validated Plans/Checkpoints have no caller-mutable raw-query import/decoder; unknown source shape and disabled current query/schema are the available refusal surfaces. This unit does not pretend to test deserialization of a forged query enum. Source review confirms the host rejects any unrecognized spatial query tag before mutation. Original checkpoint exact schema/limit compatibility, logical counters, prepaid items, non-evicting event receipts and fresh live-incarnation authority remain unchanged.

Selection computes checked uint64 distance² and checked radius² values after bounded signed-coordinate validation. Annuli add two radius-work units/binding and two selector visits/actor slot, including empty slots; disks keep one visit and original work counts. The resolved target matrix is shared across effects, and all bindings resolve before common write staging. Existing overlap/active-slow refusal, health clamps, expiry ordering, whole-step rollback, prepaid/spoken costs and explicit cancellation/no-refund laws remain. Actor16/pending8/status16, selected128, selector1024, work16384, per-plan32768 and all time/identity horizons stay fixed. No automatic retry, random target interpretation, callbacks, physics, provider or sibling dependency appears.

Independent full literal Meaning/Common and Snapshot before/release/onset/expiry checks cover centre(0,0), inner(100,0), included(101,0), outer(150,200), outside(251,0) in both actor insertion orders. Delayed tests move members over both boundaries and remove/disable/kill others; overlap checks reverse actor/effect orders. Zero-inner versus old disk-centre tests distinguish tags. Extreme ±1e6 coordinates and radii999999/1000000 exercise checked arithmetic. Failed shape/empty/status/overlap/selector/work steps preserve whole world/queue/resources, including earlier scratch activations before a later empty action. Old immutable observations stay usable.

The maximum alignment test executes four healing effects in disjoint annular bands with eight resolved bindings, observing literal51/52/53/54hp and40mana payment. It verifies eight target plus four effect rows without relying on a permanently conflicting example. Five lesson receipts coexist after the original four school productions; the targeting fifth produces known-mask511/revision5, while a tightened revision4 refuses it without change. Duplicate identity behavior remains idempotent/consistent across all five slots. Retained pending annular checkpoints outlive source/compiler/catalog/host, preserve nonzero aim/due100, invalidate old casting/event capabilities and resume an admitted M3 touch explicitly: old heal12 plus annular fire8 yields54hp and slow750 until300, one charge consumed/receipt retained. Another checkpoint captured for the old disk before new learning/book edits still includes its centre and keeps the old armed/spent item semantics after all original owners disappear. No source regeneration substitutes for retained plans.

### M5 actual commands/counts and updated resource evidence

All native work uses `/mnt/projects/cheryl-features/scripts/build --`, one job, existing `projects/magic/build` Debug C++23 GNU16 ASan+UBSan tree. Complete diagnostics/commands/immediate exits are saved in `feature-development/magic/logs/M5/`. Native CTest sets only `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`, preserving known LSan-under-tracing gap. No resource/allowance/auth refusal, unchanged retry, build cancellation or native/compiler failure occurs. Existing adaptive/no-swap/4GiB admission policy is inherited; no dependency retrieval, sibling/cache/pin/config/ref mutation or new service/subagent.

| Log | Actual command after wrapper | Result |
| --- | --- | --- |
| configure-01.log | `cmake -S projects/magic -B projects/magic/build -DCMAKE_BUILD_TYPE=Debug -DMAGIC_SANITIZERS=ON` | exit0, same incremental tree |
| build-01.log | `cmake --build projects/magic/build --parallel 1` | exit0, all old and new targets compile/link |
| accumulated-01.log | `ctest --test-dir projects/magic/build --verbose --parallel 1` | exit0,17/17 names once, including all15 M1–M4 names, initial24 new annular cases and1 ordinary caller |
| build-02.log | `cmake --build projects/magic/build --target magic_annular_checks --parallel 1` | exit0; test-only runnable maximum-alignment/five-receipt refinement |
| affected-01.log | `ctest --test-dir projects/magic/build --verbose --parallel 1 -R '^magic_annular_contracts$'` | exit0,1/1, final25 annular cases; production/caller unchanged since accumulated pass |

Five native invocations all exit0. No full regression rerun follows passing accumulated checks; only the changed contract test is rechecked. Unique CTest names are17, not18 due to focused recheck. Accumulated separate retained counts: M1 35 compiler cases,4 ordinary callers,6 input fixtures,9 CLI process scenarios; M2 37 host cases/6 callers; M3 34 item/checkpoint cases/3 callers; M4 25 learning cases/1 caller. M5 adds25 final contract cases and1 caller plus six source/Common files; these counts do not become extra CTest entries.

Measured fixture-specific resource evidence: equivalent one-target disk activation308 work/16 selector units; annulus326/32, exactly+18 work. Exact326 vs325 and selector32 vs31 preserve paid pending state on refusal. Complete eight-action/four-binding step hits unchanged selector1024 exactly (32 selected pairs);1023 rolls back the whole step. Eight bindings×16actors hit128 selected pairs exactly;127 refuses before adoption. Canonical annular compiler fixture:73 tokens,3007 work,3103 retained bytes, with exact/one-short token/work/storage and source-byte checks. Annular tome:25335 work and18937 retained bytes, exact/one-short tested. Earlier M4 measured edit work changes14823->14824 and tome work23357->23361/retained17860->17972 due to actual newly owned binding provenance; old receipts/logs stay unchanged historical evidence. No numeric quota is raised. Metadata/allocator/control-block and caller-retained history limits remain the documented logical model, not exact heap/RSS/CPU guarantees.

Static/staged review is saved separately. Runtime remains private single-thread native-owner fixture code, with current symbolic key+primitive/schema/permission checks, no physics/acquisition/renderer or production combat integration. No human comprehension/fun, graphical book, full language packs, additional effects, disk codec, cross-version/process save migration, crash/power-loss durability or external exactly-once claim. Book/knowledge records are still outside M3 world checkpoints. Other platforms/toolchains/ABIs, leak coverage under tracing, allocation fault injection/OOM and exact heap/CPU accounting remain unverified. These are acceptance boundaries, not blockers to the selected mechanical prototype.

This is the third and final magic milestone in feature-round-20261024-0500. Finish one coherent M5 checkpoint and remain idle; no fourth unit/worker/service is scheduled. Next concrete action is coordinator source/receipt/log review. A useful but **unselected** next evidence is a small human text-comprehension exercise: predict centre/inner/outer membership and shared school scope from the exact Common row, then compare with the literal fixture. Conducting it, graphical work or any new capability requires a separate assignment. Publisher owns ordinary feat/magic backup transport.


# MAGIC-M6 standalone continuation

## Pre-implementation gate and proposed caller

Exact clean base c4dbffe4523009b9f74cf103a19ee5b4ecd105b9 on feat/magic.
M5 receipt matches feature-round-20261024-0500/MAGIC-M5 ready/true; all five
complete committed native logs are nonempty, byte-equal and exit0. Source inspection
covers the compiler, learning consumer, staged host and distinct checkpoint.
Run 20261024T065550Z-d1fbd338f67b uses inherited configured model, explicit ultra,
no-swap adaptive scope; admission11823MiB available, floor4096. Standalone M6 does
not extend the completed queue. Original v3 provenance above remains the only
personal-taste evidence; these private additions are engineering choices.

Proposed caller BEFORE dependent APIs (names provisional):
```cpp
auto arch = owner.add_object({Appearance::OpenArch, true});
auto verse = compile(disguise_source, host_catalog); // show captured subject as wall
Request request{caster, {}, Point{0,0}, arch};
auto ticket = owner.prepare(*verse, request);
owner.release(*ticket); // pay10, freeze subject/aim, due100; audience remains future
checkpoint = owner.checkpoint(); // owns actual retained plan
// A moves251->100; B100->251. Destroy sources/catalog/owner; restore checkpoint.
owner.advance_to(100);
auto a_view = owner.perceive(A, arch); // stone-wall only; no bundled secret truth
assert(owner.inspect_perception().objects[arch.slot].data.passable);
owner.release(*owner.prepare(discern_plan, {caster, {}, Point{100,0}, arch}));
// Exact small disk selects only A; witness(arch generation,OpenArch,passable,tick100,instance1).
auto active = owner.checkpoint(); // keeps contribution + instance-specific witness
owner.advance_to(300); // expires before new activation; dated witness remains
```

Selected reversible policies, high confidence in ownership and medium in private
syntax/API: a typed payload variant appends Disguise/Discern to the three existing
operations; closed OpenArch/StoneWall appearances. One captured object role is
independent of audience geometry. Exactly one perception effect/activation and
burst audience only; all mixed/perception bundles and engraving refuse before
payment. Each selected observer must have an unrevealed applicable instance for
discernment; otherwise the whole activation refuses (no-applicable policy).
Witnesses never evict, repeats on an already revealed instance refuse. Physical
facts, active overlays and dated knowledge are separately owned. Owner object
edits update truth without rewriting history. Removal retires only matching
contributions/memberships, never retargets historical references.

Hard caller-tightenable ceilings:4 objects,8 instances each with16 generation-bound
audience cells,16 lifetime witness records; max instance identity1000000 and object
generations use existing max_generation. Preserve existing quotas, payment10/effect,
no-refund cancel and blocking-clock atomic whole-step advance. New fixed state
copies/scans and owned compiler variant slots are explicitly charged; exact cost
assertions may change but quota ceilings and old behavioral case names remain.
No generic perception framework, callbacks, renderer, physical collision, disk codec
or network secrecy is introduced. New ordinary results own only perceived form,
subject/observer and tick; trusted owner separately inspects truth/contributions/history.

## M6-028 — Typed compiler and knowledge boundary (implemented)

Actual source: compiler.h/compiler.cpp append Disguise and Discern to the original
three primitives. Effect::payload is monostate for all old scalar effects, or a
VisualDisguise{CapturedObject, registered Appearance, duration_ticks}, or
Discernment{CapturedObject}; legacy quantity/duration fields remain zero for new
payloads. No string parameter bag, actor-stat stand-in or executable object name.
The only new role describes the live request's separately captured ObjectId.
Common@1 clauses spell subject capture at release/recheck at activation, form,
independent duration and frozen audience at activation. Existing bindings specify
G0, actor-centre membership, exact inclusive disk/strict-inner annulus and held aim.
New source fixtures independently author disguise/discern Common. Reparse equals
complete Meaning including variant/role; changed appearance/onset/duration/shape
are independent literal tests. Existing invented Mend/Cinder clauses stay intact;
new operations use Common within the same explicit bridge. No real fantasy-language
claim or D&D edition/dice/save/concentration compatibility is made.

The registered schema set grows only3->5. Keys remain host-owned <=64 bytes,
unique by primitive/key. Disguise checks current key AND primitive, typed role,
closed form capability bit, positive duration and current max_duration; Discern
checks its own key/primitive/typed role and instantaneous zero legacy fields.
Appearance support is a two-bit caller mask (OpenArch1, StoneWall2). Source cannot
supply current object authority. Eleven rule bits (mask2047) append exactly the two
Common operation concepts to M5's511. M4 books prove recognition with unknown
Disguise/Discern concept, accepted known entry, owned failed draft preservation,
and a known plan refused by the current host. Common does not bypass concepts.
Concept availability is fixture-initialized Knowledge, as with older operation
concepts; existing five school/target lessons do not teach new concepts or mint
host tickets. No new tome, language-pack or generic content facility is selected.

Alternative: parallel perception spell parser/consumer would duplicate binding,
units, authority and checkpoint laws. Selected shared compiler/host stages preserve
those contracts. Alternative untyped enum quantities would confuse appearance and
information with hp/rate. Variant is narrow/private; future cross-domain algebra
is unselected. Current source and host explicitly refuse any bundle involving
perception plus another operation and any perception engraving before payment.
Old multi-effect heal/fire/slow composition remains supported.

## M6-029 — Physical truth, perceived result and dated knowledge (implemented)

Trusted native owner creates/edits/removes at most4 ObjectView facts. ObjectId is a
slot/generation logical reference, not a pointer; form and passability are physical
fixture facts, with fact_revision drawn from the checked world revision. Logical
references grant no release authority. Existing max_generation<=1000000 and
max_revision<=1000000000 bound object reuse/edits. Invalid forms, missing/stale
subjects and horizon/capacity exhaustion refuse without mutation.

Disguise activation allocates one checked monotonic instance (max_instance<=1000000)
with subject, form, action provenance, onset/until, actual immutable plan handle and
16 fixed actor-generation cells. At most8 active slots:128 membership cells maximum,
no growing vectors. All eligible audience centres are resolved once through shared
M2/M5 disk/annulus selection before any contribution. A changed camera/request/owner
position cannot move the held point; object reference remains captured at accepted
release. Delayed activation rechecks current subject generation and schema. Later
movement/eligibility changes do not continually retarget an accepted influence.
Removal clears only that generation's memberships; an empty contribution retires.
Object removal retires only that object's contributions. Neither removal alters
dated witness history or targets a replacement slot. There is at most one active
contribution per (subject generation, observer generation), including revealed
contributions. New overlaps refuse completely; disjoint audiences/subjects coexist.
Expiry is [onset,until), before same-tick new activations, and removes only current
contributions/retained plans; it never restores physical facts or a stale snapshot.

Ordinary perceive(observer,subject) returns an owned value snapshot with only
observer/subject/current perceived form/tick; no physical passability, hidden
influence list or unconditional true form. The host never borrows or mutates prior
value outputs. inspect_perception is explicitly a trusted-owner inspection of
facts/contributions/history; knowledge(observer) returns only that generation's
already recorded dated witnesses. These read paths grant no knowledge, mana
charge, release/event ticket or capability. They are not network secrecy APIs.

Authorized Discernment uses ordinary prepare/release, live caster permission,
payment10 and current symbolic schema. Every selected observer must have an active
unrevealed instance for this subject. No instance OR already revealed means
NoDisguise for the complete activation, before any debit for immediate release.
Capacity failure also preserves everything. Each accepted witness records observer,
subject generations, actual physical form/passability, observation tick, fact
revision and identified instance. All additions are prepared in fixed scratch
before adoption. Sixteen lifetime records never evict; ordinary failed attempts
consume nothing. Historical facts survive physical edits/removal/expiry, remain
dated, and never become current omniscience. Revealing instance1 does not reveal2.
This all-or-refuse policy is reversible fixture policy, selected to avoid partial
information/payment; general sensory tests/dispel/AI/control remain unselected.

## M6-030 — Staging, retained continuation and resources (implemented)

Release validates subject/schema/authority/funds, captures ObjectId+G0 point and
adopts payment/queue together. Existing10 mana/effect, no-refund cancellation and
blocking-clock policies are unchanged. A delayed request does not promise future
audience/conflict/instance/witness capacity at release. At advance, subject, current
eligible audience, keys/schemas, overlap/horizons and all budgets resolve on the
scratch State. Failure preserves the previous paid pending world/clock/queue and
all facts/knowledge/counters. Successful earlier scratch actions or expiry in that
same failed step are also discarded. No external callback or automatic retry exists.

State and DISTINCT opaque Checkpoint retain all M3 actors/slow/clock/actions/items/
charges/ledger/schema/limits plus object facts/generations/revisions, active
instances/audiences/expiry/provenance plans, witnesses/horizon and actual pending
perception plan/subject/aim/due. Continuation validates all bounds, canonical empty
slots, live references, historical generation bounds, duplicate instance/pair/
witness identities, schemas, exact limit identity and due/expiry times. Capture
refuses an unsupported retained plan; incompatible/default restore preserves the
destination and its old usable ticket. Restore validates before allocating/adopting
new owner incarnation; old casting/event inputs never revive. Logical instance/
replay identity differs from live authority. Deliberate whole-world rewind can
rewind knowledge/charges/counters within the restored timeline, never an external
exactly-once guarantee. Snapshot/PerceptionSnapshot remain observations, not restore
shortcuts. No disk codec, cross-version ABI/schema migration or book save is added.

Existing source8192/tokens512/bindings8/clauses4/effects4/compiler work65536/retained32768
and host actor16/pending8/status16/selected128/selector1024/work16384/plan32768 quotas
are unchanged, as are M4 book quotas. New caller-tightenable object4/instance8/
witness16/max_instance1000000 caps refuse zero/one-short/exhaustion explicitly.
The logical fixed copy model rises80->244 units:164 extra for object4 + instance8*
(16 audience cells+metadata+plan handle) + witness16. Pending subject lives in the
already accounted fixed pending slot. Checkpoint additionally scans4+8+16 slots;
extra schemas above3 charge70 each for bounded key/metadata copy/identity. Queries
charge fixed results and visited slots/witness records; normal selector work is
still disk1/annulus2 per actor slot and two annular radius units. Added type slots
are charged by sizeof(Effect) before allocation; input/retained source/gloss and
all old compiler/tome budgets remain checked. Logical units are not exact CPU,
allocator/control-block, heap or RSS accounting. Factory catalog validation/copy
is intrinsically bounded by five schemas/64-byte keys; no variable-sized world or
external callbacks are admitted. Allocation exceptions preserve existing adopted
state; allocator fault injection remains unexecuted.

## M6 executed verification and retained gaps

Actual ordinary caller is projects/magic/src/perception-demo.cpp (not the proposed
pseudocode): compile/inspect actual Common; create passable open arch and A251/B100;
pure preview; paid delayed release at held(0,0) radius250; move A100/B251; checkpoint
pending; destroy original authoring/catalog/host; restore fresh authority; tick99
arch, tick100 A wall/B arch; physical OpenArch/passable true; authorize A-only
Discernment; exact witness(instance1, tick100, fact revision4, arch/passable); checkpoint
active; destroy that owner; restore; expiry300; witness retained; immediate later
instance2 again hides the arch from A; old tick100 wall result remains unchanged
after every owner dies. No source regeneration substitutes for pending/active plans.
The caller prints actual Common and checks typed outputs/health/speed/resources;
its single success line follows the assertions, not an assumed transcript.

First seven actual wrapper invocations, all one job, complete command/diagnostics and
immediate exits saved under feature-development/magic/logs/M6:

1. configure-01.log: `/mnt/projects/cheryl-features/scripts/build -- cmake -S projects/magic -B projects/magic/build -DCMAKE_BUILD_TYPE=Debug -DMAGIC_SANITIZERS=ON`, exit0.
2. build-01.log: wrapper `cmake --build projects/magic/build --parallel 1`, exit2.
   GNU -Werror catches ObjectView{id} missing a default data initializer in the
   new owner. Fix is ObjectSpec data={}, preserving default canonical empty facts.
3. build-02.log: same incremental build after that source correction, exit0.
4. build-03.log: same incremental build after pre-run test review adds a valid
   ninth eligible actor for the full-instance refusal and alternate-form/removal
   assertions (plus include ordering/clarifying checkpoint comment), exit0.
5. accumulated-01.log: command-local ASAN_OPTIONS=detect_leaks=0 and
   UBSAN_OPTIONS=halt_on_error=1 wrapper `ctest --test-dir projects/magic/build --verbose --parallel 1`, exit8.
   **18/19 entries pass**, new35 perception cases and real caller pass. The old
   M3 case `checkpoint state work bound refuses without changing caller outputs`
   fails during fixture actor setup because work100 predates fixed copy244.
   This is not a compiler/runtime semantic failure. Correct setup allowance100->300
   permits setup yet still refuses checkpoint (at least540); assertions/name stay.
   All other previous names/behaviors pass. The initial failed evidence is retained.
6. build-04.log: wrapper `cmake --build projects/magic/build --target magic_engraving_checks magic_perception_checks --parallel 1`, exit0.
   Test-only setup correction plus five materially useful added perception cases:
   live delayed schema, captured subject/aim through edits, all-or-none witness
   capacity, full16 audience/witnesses, exact-expiry discernment blocking policy.
   Combined checkpoint case now actually holds illusion+witness+pending old
   annulus+item+outstanding event, then resumes heal/fire/slow/expiry coherently.
7. affected-01.log: same sanitizer environment, wrapper
   `ctest --test-dir projects/magic/build --verbose --parallel 1 -R '^magic_(engraving|perception)_contracts$'`, exit0, **2/2 entries**.
   **34 engraving/checkpoint and40 perception cases pass**. Original tool session
   remains queued behind the shared native lock and completes normally; no cancel,
   relaunch, resource bypass or unchanged admission retry.

No second accumulated run. Final evidence covers all19 unique CTest names through
the one accumulated run plus affected correction; do not report19/19 accumulated
or count the focused rerun as additional entries. Counts remain separate: M1
35 compiler cases/4 callers/6 file fixtures/9 CLI scenarios; M2 37 host cases/6
caller scenarios; M3 34 cases/3 caller scenarios; M4 25 cases/1 caller; M5 25 cases/
1 caller; M6 40 cases/1 caller. Old names and receipts/log bytes (including M4's
corrected failure history) remain. No sanitizer finding, admission refusal,
authentication fallback or unresolved native failure is observed.

Measured model, GNU16/Linux C++23 Debug ASan+UBSan (existing clone-local build):
- M6 disguise compiler fixture:61 tokens,2959 work,2891 retained logical bytes;
  exact/one-short source/token/work/storage acceptance verified.
- M6 delayed activation:471 complete work,16 selector visits,1 selected target;
  exact471/470,16/15 and1/0 preserve complete paid pending state on refusal.
- Retained old S2 complete activation:308->496 work. Extra164 fixed state copies
  and24 expiry scans (8 discovery +8 expiry +8 rediscovery) account for188.
  Equivalent M5 annulus326->514, still exactly18 extra work and32 selector visits
  versus disk16. Complete selector1024/1023 and target128/127 tests still pass.
- Old touch168->332 work in the initial run,333 after final charged envelope scan. Empty compatible three-schema checkpoint348->540,
  one-short539 refuses. Historical test name `exact348 and one-short347` stays
  to preserve discovery; its literals now540/539 with the explicit model comment.
- Typed variant adds16 bytes per Effect slot in this ABI: retained S1 2096->2112,
  S2 2773->2805, M5 annular3103->3135. Existing dynamic exact-limit assertions stay.
  M4 lesson retained17972->18036, work23361 unchanged; annular lesson18937->19001,
  work25335 unchanged. M4 edited-spell work14824 unchanged. CLI's now five-schema
  catalog adds121 compiler work to old fixture inspection; typed meanings stay.
- Fixed host7360, observational Snapshot1688, Observation3600 bytes on this ABI;
  these are measured sizeof witnesses, not logical quota units or portable ABI.

Passing new cases use independent complete Meaning/Common, exact geometry and
literal facts/views/witnesses, both actor insertion orders, independent alternate
form, duration/onset/audience edits, live key+primitive/form/duration/permission
refusals at release and activation, default/foreign/stale/copied/repeated authority,
subject/observer removal/reuse, capacities/horizons, overlap, pure queries and
no-applicable policy, step rollback, source/catalog/owner destruction, immutable
old value outputs, old accepted meaning and complex M3 state continuity. They use
ordinary host/book/compiler entry points; the caller shares no test helpers.

Remaining limits: private trusted native owner only; no network secrecy/authentication
sandbox, D&D edition conformance, real sensory detection, physics/collision/rays/
occlusion, rendering/audio, AI beliefs/control, general dispel/counterspell/stacking,
recurring auras, perception engraving/rearm, cross-domain composition, provider or
public Engine API. Physical passability is a fixture fact, not a physics integration.
No human clarity/learning/fun or graphical acceptance; no new playtest claim.
Known tracing restriction keeps leak checking disabled/unverified; other platforms/
toolchains/ABIs, allocator fault injection/exact RSS/CPU remain unverified. In-memory
checkpoint ownership is verified, not disk/process/crash/power-loss durability,
cross-version migration or external exactly-once. Knowledge/book save remains
outside this world checkpoint. Native value results are owned snapshots; callers
may edit their own copies, while host changes cannot alter earlier outputs.

Next concrete action: remain idle for coordinator review of standalone M6 source,
receipt and all saved evidence. A separately selected graphical observer-view
comparison would be useful next evidence for human readability; it is unselected
and no worker/unit/service/integration is scheduled here. Publisher owns transport.


### Final accounting review and affected verification

Final code review identifies one added inscription-envelope scan needing its own
work charge. It now spends one unit per effect before accepting/refusing the
entry; quotas stay fixed. This changes S5 touch332->333 (old M5 touch168), not any
physical/perception mechanic. The uncommitted receipt is explicitly held pending
until the affected executable check completes, then marked ready again.

8. build-05.log: wrapper `cmake --build projects/magic/build --parallel 1`, exit0,
   rebuilds the modified reference host and relinks dependents incrementally.
9. affected-02.log: same sanitizer environment, wrapper
   `ctest --test-dir projects/magic/build --verbose --parallel 1 -R '^magic_((engraving|learning|annular|perception)_contracts|engraving_caller)$'`,
   exit0, **5/5** affected entries.34 engraving,25 learning,25 annular and40
   perception cases plus3 engraving caller scenarios pass. Exact touch333 and
   unchanged perception471/disk496/annular514/compiler/tome metrics are printed.
   This selection covers inscription consumers/checkpoint paths; unaffected old
   compiler/host and new perception-caller evidence is retained from the one
   accumulated run. No second unchanged full suite is run.

Final total:9 configure/build/native invocations, with complete original errors,
commands/immediate exits. All selected acceptance has passing evidence after
correction; standalone receipt ready/true. Staged/static review verifies source
hashes, preserved predecessor receipt/evidence bytes, old case names, formatting,
final pass coverage of19 unique CTest names across justified runs, and no unrelated
changes. No subagent/provider/service/push/dependency change or new unit follows.

# MAGIC-M7 varied standalone batch

## Initial gate, selection and caller before APIs

Exact clean M6 a01a5f76592ed1f851de3a43a4b6462aa516a072, feat/magic. Committed M6
ready/true receipt has exact base c4dbffe; all nine complete native logs are
nonempty/committed and preserve original failures. Source inspection covers typed
compiler, first-effect guards, staged host, distinct checkpoint and M4 consumers.
Prior evidence19 unique final names/196 contract cases is retained, not rerun as a
baseline. Workspace policy, brief/backlog, design/decisions/effect-coverage,
assignment README and clone guides are read. Run20261024T074451Z-a19837bc5076 uses
inherited configured login/model, explicit ultra, admitted12380MiB/floor4096 and
adaptive no-swap scope. No personal taste is inferred from agent-authored APIs;
intentional v3 evidence above remains historical provenance.

Before new APIs, magic/batches/M7-plan.json selects Ward/Intent/Relocate/Shatter
across protection, mental restriction, movement and object destruction, plus
Transmute/Create stretches. Proposal: cast ward5, native hit3 then Fire4 absorbs3
then2 and passes2; seal an attack; break a blocking wall and relocate through the
freed destination. Book edits keep last accepted revision. Physical transmutation
and fresh owned creation are bounded stretch candidates, not implemented claims.
No catalogue sweep, generic VM, sibling integration or task controller is added.

## M7-031 first checked capability checkpoint

Implemented Ward and Restrict typed payloads have independent capacity/duration
or registered Attack intent/duration; old hp/permille fields stay zero. Common
roundtrips complete Meaning. Catalog cap explicitly grows5->7; new operation
concept bits are exact and owned. RuleMask widens to32 bits for the selected finite
batch, while numeric source/token/clause/effect/retention ceilings stay unchanged.
New mechanics remain standalone spoken contact/burst; perception bundles and all
new mechanics engraving remain refused. Keys AND primitives/quantities/durations
are rechecked, not enum-dispatched alone. Four Ward and four Restriction slots
have actor generation, action, shared checked mechanical instance, onset/until and
retained plan; max mechanical identity1000000 is caller-tightenable. No replacement/
stacking. Native trusted-owner try_attack is fixed3dp, contact50mm and no mana;
it checks alive identities and restrictions then resolves real damage through the
same absorption stage as Fire. This is not a network authority surface or combat AI.

Ward5, hit3, Fire4 independently verifies capacity5->2->absent, hp50->50->48 and
old owned report immutability. Restriction blocks that actual attack until exact
expiry, while leaving unrelated state unchanged. Selection and slot allocation
are whole-step staged; failed delayed activation preserves paid queue/clock and
explicit cancellation does not refund. Removal clears only captured generations.
Checkpoint owns partially spent capacity, restriction and actual immutable plans,
validating current schemas, unique references/identity, horizon and full duration.
Restore creates fresh authority using the existing distinct Checkpoint; observational
Snapshot is not restorable. Shared private reference-meter.h serves actual split
mechanics.cpp implementation, not a public general framework.

Evidence: configure-01/build-01 exit0. Initial core-01 exit8 is a test-fixture
eligibility error: attacker was ineligible to receive a willing contact spell.
Correct fixture makes it eligible; the exact4 area-capacity test separately marks
it ineligible to retain four intended members. build-02/03 exit0 also adds explicit
onset stamps for complete lifetime validation. core-02 passes2/2 entries:14 new
mechanics contract cases and1 independent real caller, with actual M4 add/edit/
failed-draft/revision2 plus old revision1 casting, checkpoint and resumed attack.
Complete saved commands/immediate exits remain under logs/M7. No accumulated prior
campaign yet; existing measured-cost expectations await final batch reconciliation.
Fixed logical copy rises244->260 for8 records/8 retained handles; work/selector/
actor/pending/item/disguise/witness quotas are unchanged. This first checkpoint
completes2/4 cores only. Continue the same assigned invocation through Relocate/
Shatter and feasible preselected stretches; no ready M7 receipt is claimed here.

## M7-032 second checked capability checkpoint

All four core behaviors are now implemented and focused-checked. Relocation owns
an exact signed WorldPoint (mm, G0, +/-1000000) in a typed payload; existing Point
is a compatible alias. Explicit coordinates refuse px because a scalar radius
context does not define a coordinate origin. ObjectSpec adds bounded G0 position;
old aggregate defaults remain(0,0). Native fixture occupancy means exact actor or
opaque physical-object centre, not collision volumes/rays/terrain/pathfinding.
Relocation is standalone immediate spoken contact to one willing actor. It visits
16 actor+4 object slots (plus contact1): exact21/20 selector checks. Self is excluded
from actor occupancy; other live slots block. Passable objects allow arrival and
intermediate blockers are not traversed. Attack restriction does not prevent the
same actor's real relocation. Source roles/units/ranges and unselected envelopes
refuse explicitly; no old effect implementation detects the demonstration.

Shatter is actual removal of one real fixture object, not renamed damage. A new
Query::CapturedObject and Delivery::Subject express capture at accepted release,
recheck at activation. The source holds no live object name or pointer. Catalog
cap7->9 and a downward-disable subject capability are explicit; live key AND
primitive/current-form mask/subject generation are checked before debit/activation.
The finite target matrix separately resolves an object binding with one selected
unit; it is never retargeted into an actor. Immediate and delayed spoken subject
work is supported; pending Request reconstructs no fictitious aim. One captured
ObjectId survives source/catalog/owner destruction in the distinct checkpoint.
At due100 it removes exactly that generation once; departed/replaced subject makes
advance refuse and preserves the already-paid pending state until explicit cancel.
Removal retires only that subject's disguises, never another subject or actor hp.

Actual core caller uses the accepted M4 ward revision despite a failed draft/new
revision, observes capacity, restriction and restored attack, then a real physical
wall at(300,0) refuses relocation. Shattering it permits the same compiled relocation
and changes ally position10->300 with hp45 unchanged, caster mana70->50. This is one
combined real caller, not inflated as four caller counts. Build-04/05 exit0;
core-03 runs25 cases and caller:24 cases pass, one extreme self-contact setup fails
because caster is deliberately ineligible. Explicitly marking this test's caster
eligible corrects the precondition without changing host contact laws. build-06
exit0; core-04 focused1/1 passes all25 cases. Core caller already passes in core-03;
no unchanged rerun is needed. Full inherited campaign remains pending until batch
completion; original native failure evidence stays. Four object-coordinate copy
slots increase fixed logical copy260->264, with all prior quotas unchanged.

Continue preselected material transmutation and owned creation stretches within
the same bounded invocation, then accumulated M1–M7 validation and final receipt.
These stretches are still proposals here, not completed capability claims.

## M7-033 — Both preselected stretches implemented and checked

Transmutation owns a typed registered form/duration and captured subject role.
It temporarily changes the existing physical object's form/passability, preserving
ObjectId and position. OpenArch is passable, StoneWall opaque in this fixture.
Four records retain original form/passability only, desired form, exact onset/
until, action/instance and actual plan. No old World/Snapshot is saved for expiry.
Conflicting active transmutation refuses. Native owner update_object explicitly
supersedes any material contribution and adopts the supplied complete facts;
future expiry cannot undo that edit. Shatter/remove retires the material record,
so expiry cannot resurrect it or alter a replacement generation. Intervening actor
injury/healing also survives. Partial object injury/debris/energy/thermal simulation
is not modeled; Shatter is removal of a whole registered fixture object.

Creation is a separate typed {form, G0 point, duration, selected-actor ownership}
operation, standalone immediate spoken contact. It is not relocation: checks
placement against all16 actor/4 object centres and record/object/generation/time/
identity capacity before adopting one fresh object, owner, cost10 and expiry.
Any existing actor or object centre blocks creation, even a passable object.
Four creation records join the existing four-object cap; no quota is raised.
Native trusted-owner dismiss_created requires the exact living recorded ActorId.
Wrong owner/generation refuses; owner removal leaves the object until absolute
expiry, and replacement actors acquire no control. Native actor/object IDs are
logical references within this trusted owner, not network authentication tokens.
Created object removal/Shatter retires its row; reused slots survive old cleanup.
Created objects may be transmuted, with independent lifetimes; earlier creation
expiry retires material/disguise contributions without restoring old matter.

Actual independent material-demo.cpp compiles/prints Creation/Transmute Common,
creates real wall0:1 at300 owned by the selected actor, adds an M6 disguise and
physically opens the wall. Ordinary perceive still says StoneWall, while separate
trusted inspection sees OpenArch/passable; real relocation reaches300. Checkpoint
owns original sources/plans after catalog/host destruction. Restore rejects an old
ticket, preserves physical opening through99 and restores real wall at100 without
moving/injuring the actor. Wrong-owner dismissal refuses; the exact owner removes
it. Fresh0:2 at600 survives old400 cleanup and expires500, hp50/mana50. Old tick0
perceived Wall value stays owned after both owners die. This is one second caller,
not multiple scenario counts. Closing at an occupied point does not eject/damage
occupants: the selected fixture checks destination centres, not collision physics.

Full new continuation retains Ward/Restriction/Transmutation/Creation arrays and
actual plans alongside pending subjects/aim, M3 items/charges/receipts and M6
contributions/witnesses. Validation checks version1, canonical empties, slot caps,
unique mechanical identities/actor or subject conflicts, action/generation bounds,
current keys/typed schemas/capabilities, exact durations and future expiry. Historical
creation owners may be departed with bounded generations. Restore gets a fresh
incarnation and revokes live spell/event inputs while logical IDs intentionally
survive. Default/incompatible/unsupported capture/restore refusals preserve the
old world and usable prior outputs. A combined literal test owns partially spent
ward2, attack restriction, a created/transmuted subject, pending shatter, item plus
admitted touch, disguise/witness and active slow. After original inputs die, restore
resumes touch hp50->62, material expiry/shatter at100, restriction/ward/disguise
expiry200, actual attack hp62->59, slow expiry300 and created-object expiry400;
dated witness and spent receipt persist. No source regeneration substitutes for
retained plans and no external exactly-once/disk guarantee is made.

## M7-034 — Final bounds, schemas and measured resource model

Finite operation set deliberately grows M6 five -> core seven -> nine -> final
eleven (six added operations, not21 families). Exact concept registry grows11->17
bits in owned32-bit RuleMask. Existing school/tome rules still teach only their
registered productions; each new Common operation concept is separately required
by M4 edits. Book knowledge never mints a release/event ticket or bypasses a disabled
host schema. All new operations are standalone: new/old or new/new bundles and
new engraving refuse before payment; all old scalar bundles and M6 perception
bundle refusals remain. Old Fire+Slow against a ward is tested: ActiveSlow refusal
preserves capacity5 and resources; after expiry it prevents4, leaves capacity1,
keeps target hp50 and applies new slow while the other unwarded actor loses4.

Every operation checks current symbolic key AND primitive plus its typed parameters:
Ward capacity/duration, Attack-only restriction/duration, Relocate destination,
Shatter current target/form support, Transmute desired form/duration, Creation
form/position/duration/owner role. Coordinate ceiling max_coordinate_mm is symmetric
absolute G0 mm, independently0..1000000 per relevant schema; it is not range from
caster. Both compiler and live host enforce it. A current299 cap refuses a plan at
300;300 accepts. The field is appended after appearances to preserve M6's existing
five-field schema aggregates; an actual Disguise capability refusal proves that
compatibility. No new physical frame, universal px ratio, arbitrary strings or
free-text execution is introduced. CLI fixture catalog now registers all11 and
can inspect every supported Common operation.

Old hard quotas remain source8192/tokens512/nesting8/bindings8/clauses4/effects4,
compiler work65536/retained32768; host actor16/pending8/items8/disguises8/witness16,
status16/targets128/selector1024/work16384/plan32768. Existing book quotas remain.
New downward-only caps are Ward4, Restriction4, Transmutation4, Creation4 and shared
mechanical instance horizon1000000. All active records own fixed slots/plan handles;
there is no eviction or unbounded event graph. Whole-step selected members and
simultaneous candidates count against the old work/selector/target budgets. Normal
payment remains10/effect, cancellation no refund; delayed failed advance preserves
the previously paid pending world, clock, capacity, history, objects and authority.

Measured final native evidence (GNU16/Linux Debug C++23 ASan+UBSan):

| Fixture | Complete work | Selector | Selected |
| --- | ---: | ---: | ---: |
| delayed Ward, two current eligible members |591|16|2|
| delayed Restriction, two members |635|16|2|
| immediate Relocation |477|21|1|
| delayed Shatter |612|1|1|
| delayed Transmute |611|1|1|
| immediate Creation |463|21|1|

Each exact work value and one less is executed independently, preserving complete
state on refusal. Contact1 +16 actors +4 objects yields exact21/20 occupancy selector.
Each new source independently checks exact/one-short source bytes/tokens/work/
retention; token/work/retained literals in operation order are38/2692/2158,
36/2686/2157,40/2763/2209,36/2762/2214,40/2853/2277,47/2894/2298. These use the
explicit eleven-schema fixture and exact Common text, not an implementation renderer
used to invent expected Meaning. Caller word/number edits exercise actual grammar.

Fixed logical state copy rises244->280:16 mechanic records +16 plan handles +4
object-coordinate slots. Complete steps additionally scan16 mechanic slots per
search and16 per expiry, check bounded identity pairs and count actual damage/
placement work. M2 disk activation496->584 and equivalent M5 annulus514->602 retain
exact18 additional annular work and32 vs16 selector units. M6 activation471->555;
old S5 touch333->369. Empty compatible three-schema checkpoint540->632, one-short631
refuses. Historical case name `exact348 and one-short347` stays; its new632/631
literals are explicitly explained, without changing its behavior/assertions. An
edit-helper initially expected unformatted spacing while moving the new schema
field; its guard refuses before writing. Correct line-based editing and the new
five-field compatibility test resolve it; it is not a native acceptance failure.

The largest variant grows one Effect slot8 bytes on this ABI: S1 retained2112->2120,
S2 2805->2821, annular3135->3151 and M6 perception2891->2899. M4 lesson retained
18036->18068 (work23361 unchanged), M5 lesson19001->19033 (work25335 unchanged),
M4 edit work14824 unchanged. Old dynamic exact boundaries still pass. Fixed host
sizeof8640, Snapshot1688 and Observation3600 are ABI witnesses, not RSS budgets.
Catalog and factory copies are intrinsically bounded by11 keys<=64 and fixed
metadata; logical units exclude allocator/control blocks, caller-retained history,
exact CPU and heap overhead. Allocation exceptions occur before adoption; no
allocator fault injection or deliberate memory stress is performed.

## M7 actual campaigns, command diagnostics and final limits

All20 native configure/build/check commands use the required workspace build wrapper,
one job and the existing clone-local projects/magic/build Debug sanitizer cache.
Every command, complete output and immediate status is saved in logs/M7:
- configure-01/02: actual CMake source/build/options, both exit0.
- build-01..11: actual target selections and --parallel1, all exit0. Focused targets
  grow with the checked slices;09/10 build all retained targets for the final
  campaign.11 rebuilds only new cases for old five-field schema compatibility and
  the new-ward/old-Fire+Slow transactional interaction. No unchanged rerun consumes time.
- core-01: initial focused2 entries, exit8 due to invalid fixture recipient eligibility;
  core-02 passes2/2 (14 cases+caller). core-03 exit8 with24/25 cases and caller pass,
  due to ineligible self-contact fixture; core-04 passes the affected1/1 (25 cases).
  Both failures and explicit precondition fixes remain, rather than relaxing host laws.
- stretch-01 passes4/4:46 new cases,34 old engraving cases and the two real callers.
- stretch-02 passes1/1:50 new cases after actual coordinate-schema, six work-boundary
  and full combined-checkpoint evidence. No native failure remains.
- accumulated-01 runs ALL22 CTest entries once, exit0,22/22 pass, final52 new cases.

All19 predecessor names/196 contract cases remain meaningful:35 compiler,37 host,
34 engraving,25 learning,25 annular,40 perception. M7 adds52, total248 contract
cases; callers/fixtures/CLI are separate (two new callers, six old file fixtures,
nine old CLI scenarios, retained old caller counts). No previous failure logs or
receipts are rewritten, including M4 corrected build history and M6's18/19+focused
record. Final source hashes/static/staged review bind this execution to exact
reviewed inputs; no self-referential final SHA is required. No full Engine/map/
platform/display run, native rerun after a green unchanged campaign or resource
bypass occurs. Native-lock sessions finish normally; no auth/resource refusal,
ASan/UBSan finding, provider, subagent, sibling write or direct push is observed.

Remaining limits: trusted private headless native fixture, not Engine/physics/
pathfinding/network authentication, human enjoyment/learning, graphics/audio or
edition-compatible D&D. Point occupancy is only exact centres; closing material
has no collision response. No debris/partial material damage, conservation law,
AI mind control, broad combat/action system, spell range/occlusion, necromancy,
universal dispel, arbitrary composition, recurring field, new engraving trigger
or entire catalogue is claimed. Knowledge/spellbooks are not checkpoint saves.
Current-runtime exact-schema/limit in-memory continuation and intentional whole-world
rewind are real; disk/process/crash/power-loss/cross-version persistence and external
exactly-once are unverified. Known tracing restriction leaves leak checking disabled;
other platforms/ABIs, allocator faults/exact CPU/RSS and balance remain unverified.

All four cores AND both selected stretches are checked. Finish this batch idle:
coordinator reviews the standalone receipt, source hashes and saved native evidence;
publisher alone transports feat/magic. A concrete unselected next evidence is a
human-readable presentation/prediction exercise for the two checked passage/ward
scenes before any broader integration. No extra worker, controller or task starts.


## MAGIC-M8 checkpoint1 — M8-035 / M8-036 / M8-037

Gate: exact clean9826abc on feat/magic; committed M7 ready/true receipt with expected
a01a5f7 starting head. All12 committed tested hashes and20 complete native logs are
read/verified in logs/M8/predecessor-review.json. Coordinator M7 review is inspected;
three prior commits/failure records stay unchanged. Worker wrapper records ultra.
No personal taste inference: existing private APIs establish compatibility only.

Proposed callers precede APIs in batches/M8-plan.json. M8-035 chooses four inline
remains and servant rows, downward-only caps; a present unspent remains is eligible,
within50mm of a living authorized caster. Its centre must be free of actors and
opaque objects. The fresh servant has20/20hp,0mana,1000mm/s and no casting ability.
The caster owns commands. Trusted fixture owner issues opaque command authority;
caller-supplied owner IDs on command execution cannot replace it. It is reusable
while that exact owner and servant are living/current. Death/loss makes it inert;
expiry removes only the servant generation. Dismissal/removal leaves remains spent;
active claims prevent administrative remains removal. Explicit later fixture removal
and registration can create a fresh remains generation; no automatic farming exists.

M8-036 chooses typed captured-remains / captured-influence roles and immediate
standalone deliveries. Exact Common names both spell meanings. Dispel supports only
Disguise and AttackRestriction; the closed host-owned ref contains domain, instance,
target generation and incarnation. Inspection/ref binding grants no cast authority.
Normal prepare/release rechecks symbolic key AND primitive/schema, live permission,
resources and current ref before staged removal/payment. No range/occlusion rule
for exact dispel is selected. Disguise removal removes that whole frozen audience
instance, not other instances or historical observations. New engraving, delay,
mixed composition, Slow dispelling and interruption explicitly refuse/not selected.

M8-037 adds owned LifeSnapshot internally to checkpoint State with actual immutable
plan handles. Observational Snapshot and LifeSnapshot remain non-restorable. Whole
proposed continuation checks claims, actor/owner generations, plans/current schema,
unique instance/actor/remains, caps and absolute expiry; fresh incarnation revokes
all operational refs. Whole-world rewind is intentional, not durable exactly-once.
Mutable new storage is inline fixed arrays; staging reuses bounded aggregate copies,
plan handles share existing owners. Existing standard immutable/control-block cold
allocation remains; no engine-service migration, custom allocator, copied service,
new heap registry or allocation-free/RSS claim. Command and native attack share one
Meter and one staged damage resolver. Exceptions allocate outcomes before adoption.

Executed commands: configure-01/build-01 succeed; build-02 fails in new test code
(missing aggregate fields; passing Knowledge instead of existing Learner and missing
book title). build-03 fixes those API uses. core-01 passes31/32: exact-work test
incorrectly assumes prepare cheaper than release. build-04/core-02 still fail that
case because its full-state inspection helper itself costs448 > tested441 work.
build-05/core-03 correct only the boundary assertion to available Snapshot/Life
values:32/32 cases,1/1 CTest passes. Animation preview/release both442work,
20 selector units and1 selected target in the thirteen-operation fixture. Logs
retain complete commands, diagnostics and immediate exits. No accumulated pass yet.

Empty three-schema checkpoint model changes632->652 (one-short651); historical
case name remains unchanged. This is12 fixed copy +8 empty validation units, no
raised quota. Existing scalar/annular/perceptual behavior awaits the planned single
accumulated campaign. Connected caller and combined checkpoint/history evidence
are next; no GUI/fun/AI/physics/Engine integration, leak/platform/allocator failure
or disk/process/crash safety acceptance follows from these focused checks.


M8 checkpoint2: connected-01 executes48/48 lifecycle cases and the ordinary
lifecycle caller (2/2 CTest), following successful build-06/07. The caller uses
actual M4 accepted revision1 (2000ms), revision2 (3000ms) and a failed draft;
casts revision1 under separate authority. Original source/book/learner/catalog
and host are destroyed after checkpoint. Restore retains active servant, spent
remains, Attack restriction, frozen selective disguise, A-only witness, armed rune/
admitted event and paid annular Fire+Slow due100. Old command/influence/ticket/event
capabilities refuse. Fresh dispels enable3dp attack50->47 and change B wall->arch
without physical/knowledge leakage. Rune47->59 once; B60->56/slow750 until300;
servant3:1 expires200, replacement3:2 survives; caster mana100->30->10.

M8-037 refinement: the new owned LifeSnapshot projection now charges8 result-copy
units (base292+8=300), refuses at299, and remains non-restorable. Command validates
owner/servant and resolves attack with one shared meter: minimal four-servant
fixture exact326/one-short325. Long64-byte dispel key with13-operation catalog
uses1148/1147; ordinary animation uses442/441. Exact four remains/servants and
sixteen actors, zero/downward/horizon caps, compiler input/work/storage budgets,
late failed advance preserving servant/slow/paid queue and departed-owner
checkpoint are executed. Existing fixed ceilings remain unchanged. No accumulated
campaign yet; its historical names/248 prior cases will run once at final source.

Optional M8-INTERRUPT is now selected in the owned plan after both cores pass,
with a caller recorded before dependent APIs. It reuses the existing pending
records and introduces only a typed immediate standalone spell and opaque bound
pending input. No new queue, callback registry, refund or arbitrary cancellation
surface is selected. About18 minutes elapsed at selection; the remaining runner
ceiling remains controlling. No earlier completed plan is extended.


## MAGIC-M8 final — M8-035 / M8-036 / M8-037 / M8-038

All selected work is executed: two cores plus optional M8-INTERRUPT. M8-038 reuses
the eight existing pending records; no new queue, ledger or general registry.
Common names a bound pending action, while opaque PendingRef owns incarnation,
logical action and captured caster generation. Its trusted binding query grants
no casting rights. Normal prepare/release checks current key AND primitive/schema,
permission, resources and exact still-pending identity. It may remove an admitted
cast whose former caster has departed, because it executes none of that payload;
it cannot retarget a different pending action from a replacement caster/queue slot.
Release adopts payment10, exact removal/plan release and owned interrupted_action
report together. Late work refusal preserves the already paid queue, counters,
resources and usable ticket. Prior costs, completed injury, witnesses and old
observations are never reversed. Administrative cancel remains a separate trusted
owner operation and is not called by the spell implementation. Default/foreign/
completed/cancelled/repeated/restored-old refs refuse. Snapshot stays observational.

A separate ordinary interruption caller checkpoints paid Fire action1 due100,
destroys the original owner and local damage-plan handle, restores, rejects the old ref,
binds fresh and casts interruption action2. Target stays50hp at100, caster30->20
(old release)->10 (interrupt); old queued observation stays immutable. Intentional
whole-world rewind is independently checked: restoring that earlier checkpoint
revives its paid pending action only in a fresh timeline/incarnation, where it may
activate once; no process/disk/external exactly-once guarantee is inferred.

Final controlled language deliberately registers14 operations and20 rule concepts.
New payloads are typed (Animation, Dispelling, Interruption), with no fake scalar
fields/live pointers in source. New forms stay immediate, one effect/one binding,
spoken only; delay/mixed/engraving and exact Slow/Ward/all-family dispels refuse.
Existing4effect/4clause/8binding and all actor/selector/queue/book quotas remain.
The proposed book caller detail is narrowed to Animation rather than Dispel: its
independent duration edit proves old accepted revision/checkpoint ownership while
Dispel executes ordinary compiled Common. This changes no selected capability.
No personal API preference is claimed from recent agent-authored contracts.

Final native evidence (all complete commands, diagnostics and immediate exits are
in magic/logs/M8): configure-01 and builds01..11 run through the shared wrapper,
one job; existing clone-local GNU16.2.1 C++23 Debug ASan+UBSan setup remains. Leak
accommodation is command-local ASAN_OPTIONS=detect_leaks=0, UBSAN_OPTIONS=
halt_on_error=1. build-02's test-only API errors, core-01/core-02's work-test setup
failures and stretch-01's two test assumptions are preserved. stretch-01 passes
both callers but fails2 contract entries: a new cast had superseded the ticket
whose expiry was being checked; the tight-work interrupt case incorrectly assumes
clock advance fits the same quota. build-10 fixes only those test sequences;
stretch-02 passes affected2/2 (52 lifecycle+16 interruption cases). build-11 changes
only relevant parser diagnostic wording before the final campaign. build-08/09
check progressively added optional source, without cancellation/relaunch for lock
queueing; the final build/campaign binds the saved final source hashes.

ONE final accumulated-01 campaign passes26/26 entries, including all22 M7 names.
Contract summaries are35 compiler +37 host +34 engraving +25 learning +25 annular
+40 perception +52 M7 mechanics +52 M8 lifecycle +16 interruption =316. All248
predecessor case names retain their exact order. New ordinary callers2 are separate;
prior callers,6 file fixtures and9 CLI scenarios remain. discovery-01 saves actual
CTest names. There are20 native commands total:16 exit0; build-02 exit2 and
core-01/core-02/stretch-01 exit8 remain visible. No second full campaign or unrelated
Engine/map/platform/display suite runs. Seven older receipts and predecessor logs/
provenance remain byte-preserved;37 final source/fixture hashes are checked after
execution in tested-source-hashes.json/static-review.log.

Exact resource evidence: State reserves292 fixed copy/slot units (prior280 +four
remains/four servants/four plan handles); expiry adds4 due scans and4 cleanup scans;
empty three-schema continuation adds8 checks, checkpoint632->652/one-short651.
Owned LifeSnapshot result costs300/299. In the13-schema core fixture, animation
preview/release442/441,20 selector visits (16 actors+4 objects),1 target. Minimal
one-schema fourth-servant command326/325 includes current owner/servant lookups,
record scan and shared attack resolution. A64byte dispel key/13schemas uses
1148/1147. Three-schema interruption preview339/release340; at339 prepare succeeds
but late result-publication debit/queue staging refuses intact, discard remains
usable. It selects one logical action and uses fixed pending scans charged to work,
not geometric selector units. All bounds may tighten downward; exact four remains/
servants,16 actors, zero caps, generation/instance/time/action/ticket/revision/plan/
source/token/storage/work limits and meaningful refusal-followed-by-valid-action
paths execute. Fixed accounting counts bounded slot/validation/tuple-comparison
and result groups, not individual CPU instructions or actual heap/RSS.

Allocation/lifetime remains honest: only fixed inline new mutable arrays and
bounded stack candidates are added. Retained plans share actual immutable owners;
refs share the existing incarnation marker. New owned life projections copy eight
inline records; they are neither restore shortcuts nor live authority. Existing
cold strings/control blocks/owned observations/checkpoints retain standalone
standard allocation. Allocation exceptions propagate before adoption; no Engine
allocator migration, global new/PMR replacement, copied service or allocation-free
claim. Caller-retained outputs/checkpoints have caller-controlled lifetimes outside
per-host/per-call logical quotas. No allocator fault injection or leak campaign.

Final receipt continuation/MAGIC-M8.json is ready/true as worker execution evidence,
subject to coordinator source acceptance. Private trusted fixture only; no broad
AI/combat/physics, inventory/soul/revival system, network security, GUI/human
learning/fun/balance, D&D edition, disk/process/crash/power-loss persistence or
cross-version migration claim. Fresh restore authority and intentional whole-world
rewind stay distinct from durable delivery. Worker ends idle: coordinator reviews
three coherent M8 commits/receipt/logs. A concrete unselected next proof is retained
Slow instance/effect-index provenance before exact Slow dispelling. No next task,
service/controller, sibling integration or publication is scheduled here.

Final staged review preserves raw CTest discovery JSON whitespace as generated;
source/record diff checks pass with raw logs excluded. Optional Interrupt occupies
primitive13, so the new M8 core unknown-enum refusal moves13->14 while retaining
its case name; a separate exact14-operation catalog case executes. No prior M7
refusal is removed. Initial two implementation commits are c02be2e and37aea30;
final tested hashes avoid a self-referential final commit SHA.
