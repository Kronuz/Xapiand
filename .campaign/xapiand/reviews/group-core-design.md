# group-core review: design, round 2 re-check (B7, B8)

Commit: Xapiand:e8abf4da67fd16bd9f408a2c64214211eb340b4e
Verdict: CHANGES
Date: 2026-11-17

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Scope: B7, B8 and the N10, N11, N13 and N15 folds from the re-check at Xapiand:edb610078, which
this file replaces (B1-B6 and B9 closed there or in Xapiand:6593f3522). The design is
`docs/design/cluster-release.md` at 904f8c6a7 and babacbda5 (D3, D18, Q10, section 12); the quest's
A1-A3 are at ce6a9d688 and d933f3f25. Per the lead, the design is judged as built on
`strata-nospace-position`. The 503 after three placements is the owner's call under Q10.

B8 is closed, and so are N10, N11, N13 and N15. B7 is closed for strata's verdicts. One gap remains
in the same area, which the round-2 text makes reachable: the entry's own file effects can fail
(B10). Disk full hits the volume write first, because file effects run before strata. The fix is a
sentence and an A2 case.

## Closed

| ID | Text | Criteria | Evidence |
|---|---|---|---|
| B7 | Retryable answers leave the position, and quest 5 lands after `strata-nospace-position`, whose A1-A3 run this review's probe to exit 0. A retry repeats only the strata write, and the application state updates after the definite answer. A stage-1 throw retries, and the third fences. `cluster-release.md:169-189`, D18 `:532-534` | A1 (`group-core.md:20-36`): a write whose first attempt answers `no_space`, and the `retry-whole` mutant. A2 (`:37-40`): allowance-path refusal on one replica, then freed; digests equal | Job 261117-025944-a43484: `nospace-moves` fails 36 crash points and `retry-whole` fails 40, against 0 of 1,416 for the rules |
| B8 | Re-submission comes from the original request and only for verdicts against expectations the leader chose. `invalid` answers 400 and a client's condition 409, neither re-submitted. At most three placements; a deposed proposer forwards once and the leader owns the answer. `:200-214`, Q10 `:952-987` | A3 (`group-core.md:41-50`): the invalid document, the fields-mark gap within three placements, the deposed forward, and the lift-the-bound mutant | Job 261117-025949-8a87ce: (b) gives 0 false acknowledgements, 0 divergences and 0 unanswered requests under all four rules, with 9,296 forwards under the gap. `verdict_model.py:66-121` read: per-voter applied state, and the bound counted per request |
| N10 | `no-mark` and commit-after-publication are A1 mutants, with a checkpoint after chunks or NoOps | `group-core.md:31-35` | Same bundle job: `no-mark` fails 86 crash points |
| N11 | The mark is an entry, so its retryable answers retry and a capture or close waits out `no_space`. `:180-182`; `strata-nospace-position` A2 | — | — |
| N13 | The application state's update is verdict-independent. `:159-162` | A1 mutant | `appstate-gated` fails 74 crash points |
| N15 | A stalled leader transfers leadership after one election timeout to a follower applied past the entry; a lone voter's writes time out. `:191-196` | A3 | — |

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| B10 | yes | gap | D3 covers retries of strata's answer only. "The entry's file effects ran once before the first attempt" (`cluster-release.md:179-181`) assumes they succeed, but they are the entry's first disk write: on a full disk, a blob push into a volume fails with ENOSPC before strata answers anything. Nothing says what the group does then. Only chunk bytes are positional (`pwrite` at their offset, `:404`); a non-chunked blob is "pushed into its own `DataStorage`" (`:156`), which on master appends. A partial append followed by a retry shifts this replica's locators against the log, and the digests diverge silently. A sync failure at checkpoint step 2 (`:481`) is also unhandled: a failed fsync cannot be retried safely | `cluster-release.md:156`, `:179-181`, `:404`, `:481`; `bundle_model.py` injects `no_space` only at strata's attempt (`:197-206`), never in a file effect | One rule in D3: every file effect writes at the volume and offset the entry's starting application state names (`pwrite`), so a failed file effect (ENOSPC, EIO) stalls and retries like `no_space` and rewrites the same bytes. A sync failure fences the replica (`application_failed`, repaired by install). A2's storage case also makes the file set's write fail once, and a mutant that appends on retry fails it |
| N16 | no | nit | A re-submission forwarded to a new leader must carry its placement count; if the count restarts, the bound holds only per leader. The model keeps the count in one shared map | `cluster-release.md:209-211`; `verdict_model.py:72-78` | Say that the forward carries the count |
| N17 | no | nit | The model's election keeps every entry ("the new leader holds every entry so far"), so a deposed proposer's uncommitted entry is never replaced. D4's `superseded` answer remains untested by a model; A3 should exercise it | `verdict_model.py:129-132`; D4 | A3 adds an uncommitted placement overwritten by the new term: answered `superseded` and not re-submitted by the group |

## Not run

- No code exists, so nothing was built. The two model jobs were reused from their logs and not rerun.
- `strata-nospace-position` is unbuilt; per the lead, this review assumes its outcome.
- B10 rests on the design text and on the bundle model's own scope; no model injects a file-effect
  failure.
