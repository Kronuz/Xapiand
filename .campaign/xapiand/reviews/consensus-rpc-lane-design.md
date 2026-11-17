# consensus-rpc-lane review: design

Commit: consensus:dbf559bb560133ac5f6acb50f0590435b46e39b8 (`docs/design/rpc-lane.md`, `docs/design/rpc-lane-model.py`)
Verdict: CHANGES
Date: 2026-11-16

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The shape is right: one lane on Bulk, fixed-width frames, receiver-reserved per-call credit from
a budget apart from groups', retired ids discarded rather than fatal. The frame arithmetic checks
(65,584 and 65,688 bytes, 2,048 grants per 4 GiB, 0.06%), the cited code matches
(`session.h:62-64,168,177-181`, `peers.h:373-381`, `snapshot_transfer.h:37`, `types.h:115`,
`hello.h:17`), and the model holds under two behaviors it leaves out (D1's reordering, D4's early
end), but its budgets are too small to test D5's regrant rule. Six fixes stand between it and building: five in the text, one in the model's budgets; one is
the scope split the lead relayed.

## Evidence I relied on

None for the criteria: nothing is built, and no job at dbf559b proves A1-A7. The model's own
claim is reproduced by my probe job rather than taken from the commit message.

## What I checked myself

Scripts in `.scratch/review-consensus-rpc-lane-design/` (home checkout): `run.sh MODEL fast|heavy`
runs the shipped model and its five mutants, then `probe_model.py`, a copy with three options:
`PROBE=reorder` (D1: a small frame overtakes queued chunks, calls' chunks round-robin, one call's
chunks stay in order), `PROBE=early-end` (D4: the server ends its response before it reads the
whole request; the caller's call ends at the response's end) and the mutant `split-release` (the
consumed chunk returns to the pool and the regrant is a later step).

- Job 261116-201259-c9660f (`run.sh fast`, exit 1 by design of the script): the shipped model
  reproduces 94,139 states and its five mutants each fail; `reorder` holds (129,385 states); and
  `split-release` **survives** at the shipped budgets 2/1 (276,132 states).
- Job 261116-201303-e68f5f (`run.sh heavy`, exit 0): `early-end` holds (2,880,285 states),
  `early-end,reorder` holds (6,107,525 states), and the five mutants each fail under both.
- Job 261116-202856-7420f5 (`run-contend.sh`, exit 0): with `BUDGETS=4/1`, the caller's budget
  two windows, the design holds (351,217 states; 404,633 with `reorder`), the five mutants each
  fail, and `split-release` fails (I4 at 8,227 states).

Why `split-release` survives at 2/1: the caller's first window takes its whole budget, so a second
`Call` is sent only once the first call's request has been consumed, or after it was cancelled,
when the server refuses it `overloaded`. Two calls never stream requests into the server's budget
at once, and the regrant path the design's D5 argument rests on is never contended.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | gap | **Scope: the design builds what `detent:consensus-transport` owns.** That quest's A7 (Detent `quests/consensus-transport.md`) owns the request/response lane, T1-T7, fetch as method 1 and `DTFT0001`'s deletion, and its Contracts leave only "streamed request bodies beyond it" to this quest. This design assigns all of it here: D2-D4's frames and call table, D3's versioning (T1), D7's lane-only connections (T3), D8 and Q1's fetch port, First buildable steps 1-4, the A4 row (`peers_test --fetch`, Detent's history tests) and the A6 row (fuzzing the whole `decode_rpc`). Built as written, the codec, call table and fetch method exist twice. The doc is also the only spec of these bytes (Detent's docs define no `DTRP` frame), so the fix is ownership, not deletion | rpc-lane.md:52-199, 213-223, 240-249, 274-282; Detent consensus-transport A7 and Contracts | Revision 2 marks each decision with its builder. Transport: D1-D4, D5's response credit, D7, D8, T1-T7, the codec and its fuzz target. This quest: request streaming (`Call.total`, request-direction `Body` and `Credit`, the server's early end, `Stream::write` on a request), Xapiand's methods (A4: a remote search), and A1-A3 and A5 as checks over the transport's lane. Specify the request-direction bytes now, inside version 1, so the transport's codec carries them and this quest changes no byte; delete Q1 and the fetch rows from this quest's steps and checks; state A6's scope (the call table's request-stream rules) or move it to the transport |
| F2 | yes | gap | **A regrant must not pass through the shared pool.** D5's "no grant waits on another call" holds only if the consumed chunk's release and its call's regrant are one step. The budget is per host (`rpc_incoming_bytes`) and spans the host's loops, D6 releases a chunk from the consumer's loop, and today's `ByteBudget` offers only separate `acquire` and `release` (`session.h:62-64`). Release then re-acquire lets another call, on any connection, take the freed bytes; the call is left with zero credit, and no later event regrants it, so it hangs until its deadline | Probe `split-release`: fails I4 at budgets 4/1 (job 261116-202856-7420f5); survives the shipped 2/1 (job 261116-201259-c9660f), see F8 | One sentence in D5: a call's consumed bytes move into its new grant and only the excess returns to the pool, as one operation on the budget; add `split-release` to the model's mutant list and to step 2's unit tests |
| F3 | yes | gap | **Sender memory has no local bound.** D5 bounds a writer only by the peer's credit ("its queued bytes are at most one chunk beyond what the peer granted"), and `rpc_outgoing_bytes` is named with no rule. A sender's queued bytes are then bounded by the sum of its peers' grants: 256 calls per peer at 4 MiB each is 1 GiB per peer, and each peer may grant up to its 256 MiB budget, so a generator-fed writer (A2's own shape) to 100 peers may queue up to 25 GiB. The wording also reads as if a chunk past credit may be queued | rpc-lane.md:126-127, 141-142 | Rule `rpc_outgoing_bytes`: a writer charges it before it queues a chunk and waits when it is short, or queues at most one chunk per call; reword "one chunk beyond" to "within credit". A2's high-water assertion covers the sender's queue as well as the receiver's |
| F4 | yes | gap | **No drain: a call racing a close fails with an unknown outcome.** Either end may call on a connection (call-id parity), and either closes it after `lane_idle` (D7) or on shutdown. A `Call` sent as the peer closes fails `connection_aborted`, and the caller cannot tell a call never seen from a handler that ran, while "the lane retries nothing". Every idle expiry on a lane-only pair, and every rolling restart, produces these. HTTP/2's GOAWAY (RFC 9113 §6.8) carries the last stream id processed for this reason | rpc-lane.md:105-113, 172-175, 236-237 | Owner's choice, then text: (a) a sixth kind, `Drain`, carrying the highest call id the sender accepted; calls above it fail `not_processed` and are safe to retry, which also makes `shutting_down` graceful; (b) only the dialer closes an idle connection, and the acceptor holds it while the dialer does (narrows the race, leaves shutdown); (c) keep it and state in Limits that a call racing a close has an unknown outcome. Recommend (a): one kind now costs less than a version later. Its test: the model gains a `drain` event, and A3 closes after `lane_idle` with a call in flight |
| F5 | yes | gap | **Frame validity is incomplete for an untrusted decoder.** D4 lists the closing cases, D2's table the field rules, but not: an unknown kind (6 and above); nonzero `flags` bits or zero field; a body length or trailing bytes wrong for the kind (`Credit` exactly 32 bytes, diagnostic text at most 1,024); a kind sent in the wrong direction (`Call` with the receiver's own parity, `Cancel` from the server, `Error` from the caller); an `Error` lane code (1-255) not in the table; `Call.window` = 0. "Every stream ends one way" is not yet true: with a known total, a `Body` reaching `offset + n == total` without `end` leaves the stream waiting for a zero-byte `end` that need not come. `offset + n <= total` must be checked as `n <= total - offset` against 2^64-1 | rpc-lane.md:72-96, 115-119 | One rule: a frame that breaks D2's table closes the connection, and D2's table gains the cases above; `end` is set if and only if the stream's last byte is in the frame when the total is known. A6's fuzz target asserts each (decode throws, or re-encodes byte-identically) |
| F6 | no | gap | A1 (2) fills a group's admission while calls run, but under D1 group frames share no socket with the lane except snapshot chunks on Bulk. A full group credit on a snapshot transfer is the case that could hold a call (Bulk's reader or the 1:1 alternation); D16's receiver-granted chunks make it safe on paper, unmeasured | rpc-lane.md:217; track-c.md D16, D18 | A1 (2) runs a snapshot install on the same Bulk connection with its group's credit full and asserts call latency |
| F7 | no | gap | Call ids must be allocated when the `Call` enters the small-frames queue. Allocating at `Lane::call` and then waiting for local budget (D5) lets a later id go first, and the peer closes the connection on a non-increasing `Call` | rpc-lane.md:105, 132-133 | One clause in D4 |
| F8 | yes | gap | **The model's budgets never let two calls contend.** At 2/1 the caller's one window is its whole budget, so the server never holds two unfinished requests, and a mutant of D5's central claim (`split-release`, F2) survives. At 4/1 the state space is 351,217 and the mutant dies. Beyond that, the model stands for 1-3 on one connection with one direction of calls and a budget per side; my probes add D1's reordering and D4's early end, and both hold. It does not cover one host's budget shared by its calls and its handlers, or by several connections. There a handler that waits for budget for a nested call while it holds its request's window is hold-and-wait, ended only by deadlines; fairness between peers is also open (one peer's slow handlers can take the whole 256 MiB, and every other peer is refused `overloaded`) | Jobs 261116-201259-c9660f, 261116-201303-e68f5f, 261116-202856-7420f5 | Run the shipped model at a caller budget of at least two windows (4/1) with `split-release` among its mutants; add `reorder` and `early-end` (D1 and D4 are rules the model now omits); state in Limits that a handler must not wait for lane budget while it holds an unconsumed request, or give served requests a pool of their own; optionally a per-peer cap on the budget |
| F9 | no | nit | T6's reason is off: parity is per connection and defined even with a duplicate. T6 is needed so that no call starts on the connection the tie-break closes. T4 is already the transport's own A5 ("keepalives once per connection"), so it is a dependency, not an extra ask | rpc-lane.md:208-210 | Reword T6; mark T4 as existing |
| F10 | no | nit | D5's regrant formula "`consumed + min(window, free budget)`" is ambiguous about whether the call's own remaining grant counts as free; the model uses the budget less other calls' reservations. Both readings are safe | rpc-lane.md:134-135, model 120 | Write it as the model does |
| F11 | no | nit | The lane's frames must be outside D16's Bulk connection window as well as its budgets (T2 says budgets only); small frames are FIFO among themselves, which `Credit`'s strictly-increasing rule needs; `timeout_ms` is the budget left when the `Call` is sent | rpc-lane.md:57-58, 120, 206 | One clause each in D1, D4 and T2 |
| F12 | no | nit | For the lead, not the design: the quest file's Contracts still say this quest provides the lane's frames "versioned through the `DTHL0001` hello's range", while the transport quest owns the frames and ships `DTHL0002`. A2 measures on loopback, which does not show a saturated link's queueing in front of Control's heartbeats | quest file Contracts; rpc-lane.md:218 | Lead edits Contracts; a note in A2's Limits |

Questions 1-5, briefly:

1. The header is sound and fixed-width; the five kinds with `end` as a flag are enough for calls,
   streams, credit and both kinds of end. Missing: the validity cases of F5 and the drain of F4.
   Retired ids are safe to discard: the receiver keeps its live table and the highest id per
   parity, so state stays bounded by live calls.
2. Credit cannot overrun or deadlock on one connection (model, both probes). It can stall a call
   if the regrant passes through the pool (F2), and the sender side is unbounded (F3). A full lane
   credit cannot delay a heartbeat: they share no socket and no budget (A1 (3)'s control mutant
   tests the budget). The reverse holds for Control; Bulk's snapshot chunks are F6.
3. A late `Body` after `Cancel`, a crossing `Cancel` and `end`, and a crossing `Cancel` and
   `Error` all end once at each end with no false violation, also with D1's reordering. Id reuse
   is fatal and must stay so; F7 is the way an honest caller trips it. Duplicate dials: F9.
4. Not yet: at its budgets two calls never contend, and a mutant of the regrant rule survives
   (F8). At 4/1, with `reorder`, `early-end` and `split-release` added, it is enough for the
   single-connection mechanism; host-wide sharing stays out of the model.
5. T1-T7 are the minimum, with T4 already the transport's; F4 may add a seventh frame kind,
   not a transport requirement. Deleting `DTFT0001` for fetch as method 1 is sound: its
   bound-agreement, shared-budget and unawaited-reply rules are exactly what the lane removes, and
   the outcome table maps one to one. Its "close the lane on a hung reader" rule goes; the lane
   must then free a call's serving slot at the call's end, not at the handler's return, or 256
   handlers that ignore their token refuse that peer forever. Under the scope split this is the
   transport quest's to build (F1).

## Not run

No code exists, so nothing of A1-A7 was built or timed. I did not model one host's budget shared
across connections and roles (F8), nor unknown totals, nor fairness. I spot-checked citations, not
every one (`peer-fetch.md` lines, `track-c.md:762-789` read in part). I did not judge Detent's side
of the split beyond its quest file.
