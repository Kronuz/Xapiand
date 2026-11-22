# group-heal: journal

State: waiting on jobs 261121-210217-362497 261121-210236-dca37d 261121-210245-1aac19 261121-210246-3f5652 261121-210246-ad33f8 261121-210247-218fb3 261121-210247-d363e5 261121-210247-fbfca9 261121-210248-1c5575 261121-210248-b81f77 261121-205709-52ed84 261121-204814-6859fe 261121-204824-971469 261121-214745-036576 261121-214749-38a541 261121-214752-5dffc3 (placement merges strata:cd2a4a40, the quest head, once plain 362497 passes)
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

group_voters failed at c79cf24e (261121-173201-4aa79d; CI red the same way on macos-arm64,
asan, tsan: install_gap, grow_and_heal and the ungated seeds never settle, the leader naming the
peer in installs_needed). The binding had never run green: every earlier voters job at it failed
to compile (055544, 084738, 131656, 133859). Cause, from consensus's snapshot_channel.h at
8780e44: the test's network reordered transfer frames with messages; a Chunk or Credit out of
sequence closes the transfer ("invalid sequential Chunk", "unmatched Credit"), nothing
retransmits, and the leader restarts only after snapshot_timeout, 60000 ms in the test's Timing,
past the settle's 20 s. Frames dropped by a crash or a cut stall the same way.

Fix strata:cd2a4a40 (pushed): a pair's frames keep their order (`world::in_order`), and
snapshot_timeout 3600 ms (30 election timeouts, as at consensus's defaults). Its failing test is
the plain job at c79cf24e (4aa79d). Jobs at cd2a4a40: plain 210217-362497 (A1, A2); install
mutants 210245-1aac19 210246-3f5652 210246-ad33f8 210247-218fb3 210247-d363e5 210247-fbfca9
210248-1c5575 210248-b81f77; install-silent at the regenerated mutant strata:6622d962
(refs/mutants/group-heal/install-silent) 210236-dca37d. Attribution: 204814-6859fe (first parent
2b080579) and 204824-971469 (main cc30217f). Probe 205709-52ed84 (unversioned, traced, from
.scratch/group-heal/probe-src): which of the two changes each failing case needs.

## Next

1. The jobs in State: plain must pass, every mutant fail on its named test; the probe says
   whether both changes are needed. Then `campaign verify group-heal`. Quest CI at cd2a4a40
   (run 44987690334) was still queued on runners at 21:45.
2. witness (task group-heal--witness, strata:7db919a5, pushed): CI there is its first build;
   A4 plain 214745-036576, mutants 214749-38a541, 214752-5dffc3. Its journal has the rest.
3. repair: blocked on the design ruling below. sim-rows (order 3) after repair and witness.

## Blocked on

Lead (contract): how a damaged replica reinstalls (A3, task `repair`). install's transfer
starts from the source's bundle: `fetch_pin` needs it, `serve` answers only pinned positions,
and pins are made on the source's chore at `SnapshotNeeded`. consensus's `Driver::repair(b)`
rewinds the apply index to any b in [own base, own commit] (Core::handle(Repair)), so a fenced
replica that refetches state must land at or above its own checkpoint. Options:

- (a) In place, a new strata fetch name: the fenced replica reopens, acknowledges its delivered
  ranges unapplied, asks a data voter for `repair/pin` (the source pins its latest bundle on its
  chore and answers bundle and pin), stages and activates as install does (files whose CRC32C
  matches kept, damaged ones fetched whole), then `repair(P_source)`; needs P_source in
  [own base, own commit], else retry. New wire (format-freeze reviews it); Repair is refused
  Busy while a range is delivered, so it races sustained commits.
- (b) Membership, existing contracts only: the host replaces the fenced incarnation by a fresh
  one on the node (learner installs, promoted beside it, the old one removed). A membership
  change and a whole-replica transfer per repair; host orchestration (placement).
- (c) consensus surface: a follower asks its leader for an install (a Driver call that makes the
  leader treat it as below its base). Install's path end to end, the image its new base, no
  new strata wire; Detent's library.

Recommendation: (c), one mechanism for a replica that needs state; (a) if Detent declines.

Finding for Detent (from witness, in its journal): the Driver does not expose
`Core::witness_bound`, so a witness cannot compact its log.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
