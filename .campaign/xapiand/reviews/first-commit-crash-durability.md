# first-commit-crash review: durability

Commit: 606a1d62 (strata:606a1d62d592, quest/xapiand/first-commit-crash)
Verdict: APPROVE
Date: 2026-11-23

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261123-050145-4fa570 (A2, fails-without, complete build of the head's test over strata:00eb018d's
  index.hh): first commit refuses 49 of 90 cuts ("no manifest log; seg-1.sx"), first refresh 10 of
  27, control 0. Log read; it names the refused cuts.
- 261123-050143-ad76e7 (targeted lifecycle_test at 606a1d62): first commit 0 of 117 failed, first
  refresh 0 of 81, second commit 0 of 63; the existing create sweep, T1 and rollover sweeps pass.
- Mutants 1c20df (no hook), 0c1ced (hook in commit() only), 1a9db0 (applied position and metadata
  in the empty generation), 7775a1 (hook without `buffer_size()`): each exits 1 on the CHECK or
  FAIL line it targets, after a complete build. Logs read.
- Not relied on: the full suite 261123-050400-666aa7 was still running when this review was written.

## What I checked myself

Scripts in Xapiand `.scratch/xapiand/review-first-commit-crash-durability/` (`run_probe.sh`,
`probe_block.cc`, `insert_probe.py`); source and pinned deps staged in
`.scratch/xapiand/review-first-commit-crash/`. One job, 261123-053147-44e747 (exit 0):

1. Bytes: `create()`'s manifest-1 and a deferred index's manifest-1 after its first refresh are
   byte-identical (130 bytes, same UUID), and after the commit the file is that snapshot followed by
   the generation 2 edit. So `commit_empty()` writes exactly what `create()` does, with nothing
   applied: applied position 0, revision 0, no metadata, no schema.
2. Budget flush (`buffered_docs_cap = 2`, the second apply flushes through `flush_point()`): 8
   mutations, leaving `manifest-1 seg-1.sx`. Cut at every mutation with 33 crash images each
   (process crash plus 32 seeds): 186 not created, 111 empty, 0 refused, 0 partial.
3. The quest's sweep at 128 power-loss seeds instead of 8: first commit 742 not created, 795 empty
   and 140 committed (1677 images); first refresh 734 not created and 427 empty; second commit 759
   previous and 144 committed. 0 failed.

Answers to the lead's questions:

1. Paths to a first segment. Segment files are written only by `flush_active` (index.hh:5186) and
   `flush_documents` (:5285), which only `flush()` calls (:5120-5122). Commit (:1286), refresh
   (:1205), the budget flush (`apply` calls `flush_point()` at :1019, which calls refresh or commit
   at :3515-3520) and host mode's `flush_owed` (the host refreshes) all go through `flush()`.
   `buffer_size()` counts every non-tombstone record in every buffer (:4992, :4749). A flush writes
   only the newest non-tombstone records (:4553, :5218-5221, :5167), so a segment is written only
   when `buffer_size() > 0`, which is when the hook fires. Merges and spills only follow a flush. A
   reader opened before the first commit writes nothing. A logged index throws on `apply` while
   its join is owed (:1005), and turn-log-parity keeps that (its index.hh:1029). The group's index
   comes from `create_deferred` (group.hh:162-169), and its applies flush through the same
   `flush()`. The one remaining path is `install()`: see F1.
2. `commit_empty()` (:5604) writes the same bytes as `create()` (probe 1). Its own steps are
   `roll_over` with no extra files: the temporary file, its ordering, the rename, then the
   directory sync. Every cut between them is in the sweep (12 mutations include them), and the
   rename is durable before the first segment's create. An ENOSPC on its temporary file removes
   it (:3202-3213); see F2.
3. Recovery: the diff does not touch `recover`, `uncreated` or `clear_uncreated` (:5746-5900).
   The existing check that a committed index whose logs are removed is `corrupt_index`, and that
   `create` refuses it, is unchanged and passes in ad76e7 and 44e747. `create` deletes only a lone
   `manifest-1.tmp`.
4. Cost: the guard is `!m_.generation`, which `roll_over` sets to at least 1 (:5592) and
   `recover` sets from the log, so it fires once per index. The second commit costs 6 mutations and
   3 syncs both at main (4fa570) and at head (ad76e7).
5. The sweep: faultfs flips a coin for each unsynced 512-byte sector and each namespace operation
   since the last sync, within ordering epochs (faultlab e3fcfe3c, faultfs.h:8-25). At 8 seeds the
   cuts already reach every outcome (not created, empty, committed). 128 seeds find nothing more
   (probe 3). It checks `digest()`: documents, schema version, metadata and fields (index.hh:1855-1872).
   It also checks the applied position, then commits over what the cut left and reopens to the
   committed digest.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | gap | `install()` on a deferred index writes `seg-N` copies (`write_new`, index.hh:1981) before any manifest log exists. A cut there leaves `corrupt_index: no manifest log`. Only group-heal's caller guards it, by committing first (group-heal include/strata/group/install.hh:536-538, not landed). The quest's commit path is covered; a direct `install()` caller is not. | index.hh:1961-1984 by reading; no probe | Options: put `if (!m_.generation) { commit_empty(); }` at install's start, so the core owns one mechanism and group-heal's commit becomes redundant; or keep the caller's commit and state the precondition on `install()`. Owner's call. |
| F2 | no | nit | A first rollover that meets ENOSPC on its temporary file has already moved `next_generation_` to 2 (index.hh:5569). The retry writes `manifest-2.tmp`, and a cut then leaves a lone `manifest-2.tmp`, which `uncreated()` (:5746) reads as `corrupt_index`. This predates the quest (`create()` behaves the same on main), and the journal records it. | index.hh:5566-5578, 5746-5752 by reading | Later: have `uncreated()` admit a lone `manifest-<g>.tmp` when there is no log, or keep generation 1 after an ENOSPC that wrote nothing. |
| F3 | no | nit | A deferred index's first refresh now syncs: 8 mutations where it took 2 (ad76e7 vs 4fa570). The refresh() comment ("searchable, not yet durable", index.hh:1200) does not say this. | ad76e7, 4fa570 summary lines | One clause on refresh(), or leave it to the mechanism doc, which already states it. |

## Not run

The full suite (666aa7 was still running), Debug or asserts-on builds, sanitizers, Linux/FreeBSD
and arm64/x86_64 beyond this macOS host, the prospect (inverting) layout's first-commit sweep (the
lifecycle sweep is kv_index only; code read shows `flush_active` goes through the same hook), a
crash sweep of `install()` on a deferred index (F1 comes from reading the code), storage-admission
accounting of the extra empty snapshot, a merge with turn-log-parity's index.hh, and GitHub CI
(unreachable).
