# Remaining module integration work

## Foundation

Use the current ownership, lifetime, selection and standalone contracts in
[modules.md](../development/modules.md). Reusable isolation/composition procedures
and native/fixture coverage limits are in
[architecture-validation.md](../development/architecture-validation.md).

## G6 — UI adapter

The neutral probe and controlled TGUI module checks establish the current baseline.
Complete TGUI's independent composition and native widget/runtime/lifetime
acceptance next. U9 also requires a second independent adapter; it can remain
incomplete while other independent work proceeds. Add only generic capabilities a
consumer actually needs. Adapters remain optional owners and do not select OpenGL
or own the native event pump. Use the owning
[U9 checklist](cheryl-ui-integration-plan.md#remaining-development-sequence).

## G7 — optional input composition and Steam policy

When a consumer needs composition, implement these as separate coherent units:

1. Separate source collection from publication. Contributions carry source/device
   identity into one engine coordinator; retain the current default binding scope.
2. Prove composed-input policy with fakes. Merge held contributions before deriving
   button edges, choose absolute-axis conflict policies and consume relative deltas
   once. Publish one poll with one latched focus/capture state and observed record
   order. Each effective controller has one collection owner.
3. Schedule application services independently of input admission and rendered frames.
4. Prove SDK-free Steam session/input policy before adding transport. Session
   ownership is application-scoped; context leases do not shut down another user.

**Acceptance:** Native-only behavior stays intact. Fake native + Steam sources cover
ID collisions, disconnects, focus/capture and immutable prior polls. Holding Jump
from two sources and releasing one keeps it held until the final release. A fake
application service progresses with a full input backlog, without rendered frames
and through startup/shutdown failure. Keep one provider as the default; per-player
APIs wait for a concrete consumer requirement.

## G8 — selected Steam transport and later candidates

Choose Steam SDK/version/application requirements before transport. Keep SDK-native
types and discovery inside the module. Real-client/controller acceptance remains
separate from fake policy checks. Prove controller identification and duplicate-path
suppression before promising per-device Steam/native selection; retain native
keyboard/mouse/text. Additional services, direct event mode and runtime loading
depend on later consumer choices.

Additional extraction requires a concrete omit/replace/test benefit. Another graphics
backend is a valid architectural proof; splitting engine internals simply because a
seam exists is not.

## Development boundary

Module work preserves the owner lifetimes and dependency direction in the module
guide. A future owner uses neutral Engine contracts; Engine must not acquire that
integration as a dependency.

Build/configuration/compiler/test execution follows `AGENTS.md` and requires explicit
authorization. Planning/source inspection does not imply executable acceptance.
