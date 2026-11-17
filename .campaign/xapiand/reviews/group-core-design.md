# group-core review: design, re-check of revision 2

Commit: Xapiand:68fcec825d7c6f15e33aaef65d28cbfd3e615802
Verdict: CHANGES
Date: 2026-11-17

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Re-check of the first design review (CHANGES at strata:491a1bdc, B1-B6 and N1-N9; the file this
one replaces, in git at Xapiand:f27c26cb6). Checked: `docs/design/cluster-release.md` revision 2
(Xapiand:faf31a506, 245738120: D3, D4, D11, D12, D16, D18, Q10) and the quest file's rewritten
criteria (Xapiand:4cda55a1e), both at the quest branch head above; strata is still at 491a1bdc.
Settled parts were not re-reviewed. Q10 is the owner's call; nothing here blocks on it.

B1-B6 are closed in the text and the criteria. The two rules the models added are sound for
recovery. Choice (b) has no safety hole: no acknowledged success is refused under any prepare rule.
It has two gaps, both of which a contributor would hit while building A2 and A3:

1. Its per-replica half cannot be built on strata as pinned. One `no_space` moves strata's position,
   so the retry D3 requires is answered `skipped` (B7).
2. Its re-proposal has no verdict class, no bound and no owner once the proposer is deposed (B8).

The quest file also still states revision 1's bundle contract next to revision 2's (B9). Each fix is
text plus a named test. B7 also needs a one-line strata core change outside the quest's files.

## B1-B6, closed

| ID | Text | Criteria |
|---|---|---|
| B1 | Two positions, fail closed if P_b > P_H, replay above P_b with strata skipping at or below P_H, reads wait for P_H: `cluster-release.md:422-427`, `:457-463`. Capture between entries: `:446-449`, D18 `:500-503`. Sync and commit before publish: `:450-452` | Quest A1 (`group-core.md:18-31`): both windows as crash points, the replay-from-P_H mutant |
| B2 | Apply retires, never unlinks; unlink after a bundle omits the file, then a directory sync: `:381-383`, `:453` | A1: the staging case, the unlink-at-apply mutant |
| B3 | Opaque, length-prefixed application-state section: `:438` | A1: counter, current volume and open uploads in it; the aborted-upload case |
| B4 | Total apply for per-replica verdicts, semantic refusals as committed no-ops, `quorum` carries apply's verdict, the ready point: `:169-183`, D4 `:187-194`, Q10 `:912-940` | A3 (`group-core.md:34-39`): the merge/conditional-write schedules, the at-commit and before-ready-point mutants. B7 and B8 below |
| B5 | strata's catalog owns the group and index lifetime, and its commit chore is the checkpoint; idle cost stated per part: `:332-337`, `:349-354` | A6 (`group-core.md:47-52`) |
| B6 | Recovery's rules hold under power loss and process crash | A1: faultfs under strata, the file set and the journal host; a second crash in recovery; the oracle replays the recovered log from empty; the publish-before-sync mutant |

## The two rules the models added

- **File effects regardless of strata's verdict** (D3 `:155-159`, D18 `:500-501`): sound for
  recovery. Replay skips strata at or below P_H, where the verdict is gone, so a verdict-gated file
  effect shifts later locators; `verdict-gated` fails 8 crash points (job 261117-024523-dd3b4d). Its
  live side is open: an entry whose strata write is retried after an owed or `no_space` verdict must
  not run its file effects twice (B7).
- **The capture moves strata's position to P_b** (`:446-449`): sound. Entries in (strata's
  position, P_b] carry no strata operation, so the mark changes no document. It makes P_H ≥ P_b once
  step 3 commits, and it leaves replay above P_b unchanged. `no-mark` fails 72 crash points (same
  job). A1 names no mutant for it (N10), and the empty transaction has costs (N11).

## Evidence I relied on

- Jobs 261117-024523-dd3b4d (`bundle_model.py`) and 261117-024526-a92827 (`verdict_model.py`) at
  Xapiand:faf31a506, exit 0, read from their logs. The rules fail nowhere, and each broken variant
  fails somewhere. Under (b), all four prepare rules give 0 false acknowledgements and 0 fences.
- strata `include/strata/core/index.hh` at 491a1bdc (`apply`, `evaluate`, `lane_fits`, options);
  strata:8e919415 `docs/design/standalone-turn-log.md` (document-gate D2, D6). Clean room: no GPL
  source read.

## What I checked myself

- Job 261117-025140-4bbcdd, `.scratch/review-group-core-design/verdict_position_probe.py STRATA
  COMMIT`. It walks strata's `apply()` and reports which verdicts return before the position move.
  It exits 0 when no per-replica verdict moves the position. At 491a1bdc it exits 1: `no_space` at
  `index.hh:860` moves it. Rerun it on the fix.
- `verdict_model.py` read in full. Its single state plays every leader, so a deposed proposer never
  applies a refused merge, and the drain caps retries at four rounds (B8).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| B7 | yes | defect | D3's per-replica rule ("`no_space` stalls that replica's apply and reports it. None is ever skipped", `cluster-release.md:169-171`) cannot hold on strata 491a1bdc. A `no_space` from the commit-allowance check after stage 1 falls through to the position move, so the retry at the same position answers `skipped` and that replica's applied position passes a committed write it never holds. Its digest diverges silently. The other `no_space` paths and the owed verdicts return before the move. Document-gate never meets this, because only an accepted transaction joins its log (standalone-turn-log D2). Two retry rules are unstated: (1) re-running an entry would push its blobs again, but D18 orders the entry as file effects, then the strata write, then the application state; (2) stage 1 can throw with the position unmoved (`index.hh:790-793`), which is per replica too | Job 261117-025140-4bbcdd: `index.hh:860` sets `no_space`, then `:878-880` moves the position; `:803-806` skips at or below it | (1) strata's `apply` leaves the position unmoved on that `no_space`, as at `:839` and `:844`. This edits `include/strata/core/index.hh`, outside group-core's files, so the lead routes it (a strata quick fix, or the file added to the quest) with a strata test; the probe exits 0 after it. The alternative is that the group fences the replica and reopens it from P_H on that verdict, which costs a reopen per refusal. (2) D3 and D18 state that a retry repeats only the strata write, and an entry's file effects run once, or as positional writes at offsets from the entry's starting application state; a stage-1 throw is retried like an owed verdict. (3) A2 gives one replica a storage budget that refuses at allowance growth, then frees it: digests stay equal. A mutant that re-runs the whole entry, file effects included, fails it |
| B8 | yes | gap | Under (b), re-proposal is unspecified. D3 (`cluster-release.md:180-181`) has "the leader" prepare a refused merge again "and propose it once more". Three things are missing: (1) the verdicts it applies to (an `invalid` re-prepares into the same refusal); (2) a bound (a predictor gap re-proposes as long as the gap lasts, each attempt a committed log entry; the model's `versions+start` rule makes 547 re-proposals over 360 schedules); (3) what a proposer does when it applies its refused placement after losing leadership, since the `quorum` waiter lives on the proposer (D4 `:187-190`) and only a leader prepares. Safety holds; liveness and the reply do not | `verdict_model.py` (`apply_through` re-calls `prepare` on a state shared by every leader; a four-round drain); job 261117-024526-a92827 | Name the re-prepared verdicts (`version_conflict`, `fields_conflict`, `schema_conflict` against expectations the leader chose). `invalid` answers 400 and a client's failed condition answers 409, neither re-proposed. A deposed proposer forwards the re-prepare to the leader once (D8) or answers a retryable failure. Bound the attempts and state the answer after the bound (a retryable 503 keeps Q10's "a merge is never answered 409"; that is the owner's call). A3 adds the `versions+start` predictor gap: the bound holds, the answer matches, and no acknowledged success is refused |
| B9 | yes | defect | The quest file states revision 1's bundle next to revision 2's. Outcome (`group-core.md:11-12`): "a bundle that names the strata manifest", "recovers from the bundle's applied position". Provides (`:72-74`): the manifest's position, term and CRC32C plus the file set, with no application state. Contracts (`:66-68`) and D16's table (`cluster-release.md:430-441`) say P_b and term, the file set, the application state, and no manifest CRC32C. Consumers and `format-freeze` read Provides | `group-core.md:11-12`, `:66-68`, `:72-74` | Restate Outcome and Provides from D16's table and the two positions |
| N10 | no | gap | A1's mutants omit the second new rule: no mutant skips the capture's mark (`no-mark`, 72 failing crash points), and none commits strata after publication (`commit-after-publish`, 22). A1's scripts reach the no-mark case only if a checkpoint follows entries with no strata operation | `group-core.md:28-31`; job 261117-024523-dd3b4d | Add both to A1's `--fails-without` list, with a checkpoint right after chunks or NoOps |
| N11 | no | nit | The mark is an empty transaction. At 491a1bdc it creates a buffer (`index.hh:4272`) and bumps the visible position. On an empty buffer under a storage budget it reserves `segment_overhead`, 4,096 B (`:851`, `:3935`). It can answer `no_space` with the position unmoved (`:844`), or `flush_owed` under `defer_io`, so the capture inherits apply's verdict handling and can stall a close. No strata test applies an empty transaction at a position | `index.hh` at 491a1bdc | State that the capture handles the mark's verdicts as apply does, or give strata a position-only advance |
| N12 | no | nit | D3 says every semantic refusal moves strata's position (`cluster-release.md:173-174`), but the cap `invalid` returns before the move (`index.hh:822-824`). This is harmless under the mark. That `invalid` depends on `buffered_docs_cap` and `lane_chunks_cap` (`index.hh:534-540`, "Tests lower it"), so it is a function of the log only while every replica shares them | `index.hh` at 491a1bdc | Correct the sentence; state that a group's replicas share strata's caps |
| N13 | no | nit | Only file effects are stated verdict-independent. The application state's update must be as well (D18 orders it after the strata write), or replay at or below P_H diverges | `cluster-release.md:155-159`, `:500-502` | One clause in D3 |
| N14 | no | nit | Quest A2 says three voters, while design section 5's A2 says three and five, with seeds and crash points stated (first review N8). Quest A4's parenthesis is unclosed | `group-core.md:32-33`, `:41-42`; `cluster-release.md:621-623` | Align A2; close the parenthesis |
| N15 | no | nit | A `quorum` answer completes at the proposer's apply, so a leader stalled on `no_space` holds every `quorum` answer of its group while its followers apply | D4 `:187-190`, D3 `:170` | Consider stepping down on a sustained stall; state the behavior either way |

## Not run

- No code exists; nothing was built or tested.
- B7 rests on strata's source and the probe's walk of it. No strata test drove an allowance-path
  `no_space` and its retry.
- The models are Python. They check the recovery and acknowledgement rules, not an implementation.
  `verdict_model.py` has one state for every leader, so it never reaches B8's deposed-proposer case.
- N1's `close_group` guard and N8's journal pin remain the lead's to route, as revision 2 says.
