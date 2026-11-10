# manifest-log review: security

Commit: strata:e0508e9805e612ac78b0475d6db6817a04c716c8
Verdict: CHANGES
Date: 2026-11-10

APPROVE: lands as is, at this commit's code. CHANGES: lands after each blocking finding is fixed
and confirmed by a job at the fixed commit. REJECT: the approach is wrong; the quest is re-planned.
An APPROVE lists no blocking finding; a CHANGES lists at least one.

Threat model (the lead's): a trusted cluster, so honest faults and on-disk corruption, not a crafted
log. Journal's CRC framing catches corruption. What remains for the decoder is memory safety on any
bytes, bounded memory and time on honest logs, and no damaged or honest log read as something else.

## Evidence I relied on

- Fuzz 261109-154845-6a1734 at strata:9d4630c (ASan+UBSan on the target TU, which includes the
  header-only decoder): fuzz_manifest_log ran 3,163,120 runs (cov 1664, 157 units) and
  fuzz_manifest_log_file ran 681,274 runs (cov 2086, 748 units, still adding units at the end),
  300 s each, with no crash. `git diff 9d4630c..e0508e9` leaves `manifest_log.hh`, `files.hh` and
  both fuzz targets unchanged, so this job stands for the decoder at the reviewed commit. The rerun
  261110-021407-fd1e2f at e0508e9 was still queued when I wrote this.
- A7's bit-flip and crash-image stages (build-test 261109-143551-b9d053 at strata:4d2a541) for "no
  damage reads as whole". `manifest_log.hh` changed after 4d2a541 only in D42's deleted set and
  check_edit, and the record fuzz target covers those.
- The contributor's own "deleted sets" stage (manifest_log_test.cc:1121-1187): 128 one-ordinal
  edits on a clumped dense set of 20M documents cost 15.09 ms, about 0.12 ms per edit. I use it as a
  per-edit cost below.

## What I checked myself

Read by hand (manifest_log.hh, files.hh:101-165, fields.hh:104-170, index.hh:178-195, 4842-4950,
5164-5240, 5311-5330, journal framing.h:226-311 at journal 5bbeedc):

- `detail::reader`: every integer is shortest-form LEB128; `count(min)` bounds n by bytes left
  divided by min; `bytes(n)` checks n before use. The min sizes are right: segments 25, deletes 5,
  declarations 5 (field_problem rejects empty names), metadata and attributes 2.
- `get_entry`: the bitset size must equal ceil(doc_count/8), and bits past doc_count are refused.
  The form (words or ordinals) is fixed from `dead` before anything is allocated. A deltas body must
  satisfy dead <= bytes, and its body is taken before the words vector is sized. Dense ordinals are
  range-checked before the write (`:457`), sparse ones by the last of an ascending run (`:468`). The
  canonical-encoding check is at `:470`. Memory stays within 8x the body plus a word. A4 holds.
- `get_ordinals`: strictly ascending, with an overflow check, and the reserve is bounded by the
  caller's count.
- `check_edit`: every running sum is checked (`add`) or provably non-negative, and every lookup is
  O(log n). `apply` runs only after the whole check, so a corrupt_index leaves the state unchanged.
- `read_log`: it refuses an old manifest, needs a whole first record, checks the snapshot's
  generation and UUID, and applies 2x D7's bound per edit. A damaged tail is `log_damage` without
  `to`. `recover()` throws on a bad newest log and never falls back to an older one silently
  (index.hh:5197-5207). Risk 3 holds on the read side, apart from F2.

Probes written as jobs under `/Users/kronuz/code/Kronuz/Xapiand/.scratch/review-manifest-log-security/`,
all queued and not run when this review was handed back (about 20 jobs ahead):

- 261110-022216-4197e2 (`run.sh`, which builds `replay_cost.cc` and `enospc_bounds.cc` into the
  job's checkout):
  - `replay_cost` reads a log at the writer's 1x edit bound and the reader's 2x bound against the
    snapshot alone. The shapes are clumped dense, sparse just below the threshold, and half deleted,
    each at 5M, 20M and 60M documents.
  - `enospc_bounds` is a kv index on faultfs with a 30 KB metadata value. With capacity set to
    used + 24 KB, it runs 300 one-delete commits, then closes and reopens.
- 261110-022333-b11f8f (`cov.sh`): a coverage build of both fuzz targets, 150 s each. It prints the
  lines of `manifest_log.hh` that each target leaves at 0.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | gap | The open's replay cost is quadratic in the deleted set. D7's edit bound max(R, n, S/c) assumes c = 512 snapshot bytes per edit (design :390-392, "0.18 µs per edit"). A one-ordinal delete edit instead costs a walk over up to all the set's words (`log_deleted::below`/`above`, manifest_log.hh:180-233, dense clumped) or a copy of all its ordinals (`insert` merge, :126-129, sparse below the threshold). Either is O(S) per edit, so replaying S/c edits costs O(S²/c). At 20M documents clumped, S ≈ 2.5 MB, so the bound is 4,883 edits at 1x and 9,766 at 2x. At the measured 0.12 ms per edit that comes to about 0.6 s and 1.2 s, against about 15 to 24 ms for the snapshot alone (25 to 50 times it). It grows with the square of the segment: about 15 s at 100M by the same arithmetic. These numbers are extrapolated, not measured. The design's SR2 claim (:393-395, "replay cost no more than a small multiple of the bytes it reads") does not hold for this honest workload: deletes of old documents clumped at low ordinals, then one-document deletes per commit. The T7 row measures only 128 edits. | Code at manifest_log.hh:126-129, 180-233 and 1062-1065; the 15.09 ms figure in 261109-154842-e89e93. The probe that settles the magnitude, `replay_cost` in 261110-022216-4197e2, has not run. | Settle the magnitude with the probe. Then either count each delete edit's walk or merge cost (in words or ordinals touched) against the replay bound, in both the writer's roll test and read_log's 2x check, so replay stays O(S). Or give the dense set a word summary and the sparse set a structure with O(k log n) inserts. Or record the cost as accepted under D42 with the owner. |
| F2 | yes | defect | Under sustained ENOSPC the writer's bounds are soft but the reader's are hard, so an honest index can become unopenable. An unforced rollover whose temporary meets ENOSPC returns false and the edit is appended (index.hh:4929-4931), and `rollover_owed_` is not set, so every later commit does the same. Nothing caps the edits appended past 1x. Once a log passes 2x D7's bound, `read_log` throws corrupt_index "past its bounds" (manifest_log.hh:1116), and the next open of a log written only by the honest writer reports it as damage. This is reachable when free space is below S but above the edits' bytes: a large deleted set or metadata, with deletes or metadata-only commits. | Code: index.hh:4878-4885, 4921-4931; manifest_log.hh:1062-1065, 1116; design :432-435 ("the rollover waits for the next commit"). The confirming probe, `enospc_bounds` in 261110-022216-4197e2, has not run. | Make the writer hold the reader's limit: past 2x minus one edit, the rollover is forced, and its ENOSPC refuses the commit as no_space. Or reserve S in the commit allowance once the log passes 1x. Or loosen the reader to accept what the writer can write. A test should commit under a capacity below S until past 2x, then reopen. |
| F3 | no | gap | Paths the fuzz targets cannot reach: read_log with `to` below the head and damage after it (`damaged`, `generation_not_retained`); a non-default `readable_by` (check_readable's unsupported paths for mask and versions); the fallback logic in recover(). fuzz_manifest_log_file mutations almost never pass the framing CRC, so beyond the 48 seeds' records it fuzzes the framing and caller rules, and the record decoder's depth comes from fuzz_manifest_log alone. Neither target exercises log_within's 2x check with many edits. These are control flow over checked inputs, not memory safety. The exact lines are pending the coverage job 261110-022333-b11f8f. | fuzz/fuzz_manifest_log_file.cc:45-61; fuzz/fuzz_manifest_log.cc | Optionally take `to` and a readable_by mask from input bytes in the file target. |
| F4 | no | nit | Memory amplification of decoded sections is linear but large: a 2-byte metadata pair becomes a std::map node of about 100 bytes, and a 5-byte field declaration becomes a map node plus a vector entry of about 150 bytes. It is bounded by file size, so it is not an unbounded allocation. | manifest_log.hh:715-726; fields.hh:162-166 | None needed under this threat model. |
| F5 | no | nit | Pre-existing on main: open_segment sizes its deleted bitmap from the log's doc_count (`(r.doc_count + 63) / 64`) before the segment file's doc_count is checked. For a segment entry with no deletes, doc_count is not bounded by any body. CRC-protected records make this unreachable without a writer bug. | index.hh:2459 (same on main), 5317-5319 | Optionally check the doc_count against the file before sizing. |

## Not run

- Neither probe job ran, so F1's magnitude and F2's reachability rest on code reading and the
  contributor's per-edit figure. The lead confirms or refutes them by rerunning 261110-022216-4197e2
  at this commit, and confirms a fix by rerunning it at the fixed commit.
- I did not run the coverage job (261110-022333-b11f8f) or the fuzz rerun (261110-021407-fd1e2f) at
  e0508e9.
- I ran no sanitizer build of my own. Journal's framing scan and its tail rule belong to journal and
  are fuzzed there (journal fuzz/fuzz_framing.cc); I read them but did not test them.
- I did not check Linux, FreeBSD or arm64.
