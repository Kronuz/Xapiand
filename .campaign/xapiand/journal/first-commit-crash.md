# first-commit-crash: journal

State: waiting on jobs 261123-053640-93a0e0 261123-053633-726a56 261123-053645-dd3621
Quest: .campaign/xapiand/quests/first-commit-crash.md (the lead's; never edit it)

## Doing

Code done at strata:606a1d62. A2 passed. Waiting on four A4 mutants
and the suite (A1, A3). Branch not pushed: github.com unreachable from this host (port 443 refused).

Review F1 folded in at strata:4955a679: install() commits the empty first generation before its
copies; install sweep added; F3 comment on refresh(). Jobs: F1 sweep over main's index.hh
261123-053640-93a0e0 (fails-without), targeted 261123-053633-726a56, mutant no-install-guard
261123-053645-dd3621. Not pushed (github.com unreachable).

## Next

1. Read each job's full log (~/.scratch/campaign/jobs/<id>/a1/log): A2 names the refused cut; each
   mutant fails on the CHECK it targets, after a complete build.
2. Fill the Evidence table from `campaign brief`; State: ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261123-050400-666aa7 at strata:606a1d62 (pending); targeted lifecycle_test 261123-050143-ad76e7 passed: first commit 13 cuts x 9: 50 not created, 56 empty, 11 committed, 0 failed; first refresh 9 x 9: 50 not created, 31 empty, 0 failed |
| A2 | 261123-050145-4fa570 passed (fails-without, head's test over main's index.hh, complete build): first commit refused 49 of 90 cuts, e.g. "FAIL first commit cut at 1/9 (process crash): refused: no manifest log; seg-1.sx"; first refresh 10 of 27; control 0 failed. Earlier 261123-045326-9bf051 at strata:c18becbc refused 49 first-commit cuts ("no manifest log; seg-1.sx") but its control was buggy (fixed in strata:04dcb24d): superseded |
| A3 | build-test 261123-050400-666aa7 (pending); ad76e7: second commit 7 x 9: 52 previous, 11 committed, 0 failed; existing create sweep and T1 unchanged and passing |
| A4 | mutants (pending): no-first-manifest 261123-050148-1c20df, commit-only 261123-050150-0c1ced, applied-early 261123-050152-1a9db0, always-empty 261123-050154-7775a1. Submit script: Xapiand .scratch/xapiand/first-commit-crash/submit.sh |
| other | 261123-045814-05fc0d failed at strata:89f9f5e1: the logged sweep aborted, a logged index refuses applies before its first commit; sweep and the unreachable attributes carry removed in strata:606a1d62 |
| A5 | docs/mechanisms/manifest-log-file.md, "The first manifest" and "Files no manifest names" |

## Result

strata:606a1d62 (from strata:00eb018d): `flush()` rolls over the empty state as generation 1
before a deferred index's first data file (`commit_empty()`, include/strata/core/index.hh);
recovery unchanged. Tests in test/lifecycle_test.cc (first commit, first refresh, second-commit
control, create's one generation). Doc: docs/mechanisms/manifest-log-file.md "The first manifest",
"Files no manifest names". Open: a lone `manifest-2.tmp` (a first rollover retried after ENOSPC, then cut) still
reads as corrupt, as on main.

## Rulings I made

- Ordering, not recovery. A directory holds an index exactly when it holds a manifest log; the
  first manifest goes before any data file. Recovery stays as it is, so the existing lifecycle
  check (a committed index whose logs were removed is corrupt_index, and create refuses it) holds
  unchanged (A3), and nothing is ever deleted from a directory with no log but a lone
  `manifest-1.tmp`. Recovery's alternative ("no log means uncreated, whatever segments it holds")
  would have read that damaged index as never created and let create delete its segments.
- The hook is in `flush()`, not `commit()`: refresh() and a budget flush also write a deferred
  index's first segment before any commit, the same window. Condition: no generation committed and
  documents buffered, so create() and a first commit with nothing buffered still write one
  generation.
- The empty generation is the state create() commits (UUID, configured, nothing applied), never
  the applied scalars, metadata, schema or fields: those would make a cut reopen a partial state.
  No join attributes: a logged index refuses applies before its first commit (D7), so its first
  flush writes nothing and never reaches commit_empty() (05fc0d showed the refusal).
- Cost, measured in the sweep (ad76e7 vs 9bf051): a deferred first commit of two documents takes 12
  mutations and 6 syncs, was 9 and 4; once per index. Recorded in the mechanism doc.
- install() into a deferred index takes the same guard (review F1, lead's ruling): the core owns
  the one mechanism; group-heal's commit before install becomes redundant.
