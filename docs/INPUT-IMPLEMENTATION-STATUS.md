# Input implementation status

Prepared on 2026-09-29 from original task base
`d690266c389f08e74efe0481c5fc6b744870ea29`.
Source implementation is prepared; build, automated-test, and real-platform
acceptance remain open. Compilation and test execution were excluded from this
patch-preparation pass by request.

The broader runtime/resource/render continuation is tracked in
[RUNTIME-IMPLEMENTATION-STATUS.md](RUNTIME-IMPLEMENTATION-STATUS.md). Its additional
source work does not close the input execution gate below.

| Work item | Prepared implementation | Validation state |
| --- | --- | --- |
| 1. State and simulation timing | Independent simulation scheduling; whole-batch consumption; press/release counts, observed down-time, active and completed hold durations; absolute and relative axes. | Source reviewed; test cases authored, not executed. |
| 2. Polling policy | Lockstep, finite completed-poll capacity, and unlimited backlog; completion-to-next-poll spacing; backpressure without dropping completed polls. | Source reviewed; capacity and spacing cases authored, not executed. |
| 3. Events | Ordered physical records with shared sequence numbers, observation times, repeats, modifiers, pointer motion, fractional scroll, and sampled controller changes. | Source reviewed; capture and handoff cases authored, not executed. |
| 4. Text | Independently requested OS committed Unicode scalars, retained in order with physical editing controls. | Source reviewed; channel lifetime and Unicode cases authored, not executed. |
| 5. Focus and routing | Scoped focus leases, ownership epochs, exclusive/pass-through keyboard routing, and simulation-side filtered views. | Source reviewed; ownership and gating cases authored, not executed. |
| 6. Integration | GLFW and Gainput State ordering, explicit adapter capabilities, callback failure propagation, runtime handoff/cleanup, demo textbox, and updated contract documentation. | Source reviewed; adapter and both-runtime-mode cases authored, not executed. |
| 7. Acceptance | Build, automated tests, and real demo/runtime checks. | Not performed; required before treating the implementation as runtime-validated. |

Static source parsing and Git whitespace checks do not establish C++ type,
link, threading, or platform correctness. Mailbox replay checks the deliverable's
commit sequence and resulting tree, not runtime behavior.

## Remaining acceptance gate

When compilation and execution are authorized, build the supported target
configurations and run the updated input, polling, capture, routing, adapter, and
runtime cases alongside the existing regression suite. Then check the real
GLFW/OpenGL demo in sequential and concurrent modes:

- Verify one update per simulation cycle, short taps, multiple transitions,
  long holds, unchanged observations, and updates with no new poll. Movement
  using observed down-time should stop at release without splitting a cycle.
- Exercise default lockstep, small finite capacities, unlimited polling, zero
  spacing, and nonzero spacing. Deliberately slow simulation and presentation;
  confirm capacity counts every completed poll, polling resumes after whole-batch
  consumption, and rendering/recycling can continue while polling is paused.
- Check key repeats, cross-device chords, pointer deltas, fractional horizontal
  and vertical scrolling, controller changes, and retained record ordering.
  Request/release Events and Text independently, including overlapping leases.
- Use the F2 textbox with OS-layout text, repeated characters, Backspace/Delete,
  arrows, Home/End, Enter/Escape, and non-ASCII committed input. Check exclusive
  and pass-through focus, focus transfer with pending input, reused target IDs,
  and held-key behavior when keyboard gameplay resumes. The demo's ASCII font
  preview should not be mistaken for loss of captured Unicode data.
- Resize and close during polling pressure and slow updates. Check stop,
  initialization/poll/update/render failures, callback detachment, worker join,
  frame recycling, focus release, pending-input cleanup, and resource teardown.

Record the configurations, commands, and observed results when this gate is run.
The gate remains open until those checks succeed and any findings are resolved.

## Contract limits

State durations use poll observations; transitions inside one poll retain their
counts but have no recoverable hardware timing. Physical records preserve what
the backend reports, and sampled controllers cannot expose unseen intermediate
changes. Capacity limits completed polls, not the number of callbacks inside
one pump. Text provides committed Unicode scalars, without a composition/IME,
grapheme, clipboard, font-shaping, or complete editor contract. These boundaries
are detailed in [input-state-model.md](input-state-model.md).
