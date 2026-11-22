# group-heal: journal

State: waiting on jobs 261121-210217-362497 261121-210236-dca37d 261121-210245-1aac19 261121-210246-3f5652 261121-210246-ad33f8 261121-210247-218fb3 261121-210247-d363e5 261121-210247-fbfca9 261121-210248-1c5575 261121-210248-b81f77 261121-205709-52ed84 261121-204814-6859fe 261121-204824-971469
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
   whether both changes are needed. Then `campaign verify group-heal`.
2. repair, witness (order 2, on install's contract, now merged), then sim-rows (order 3).

## Blocked on

Nothing.

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
