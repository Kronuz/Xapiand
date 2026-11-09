# threadpool-phase7c-l2: WorkPool ruled waits, work conservation and merge aging (L2)

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at threadpool:e961e44 (2026-10-29); its ledger `docs/status/phase7c-l2.md` in Kronuz/threadpool is the record. The quest's repository is `../threadpool`. Its machine state is in `state.json`: `campaign status threadpool-phase7c-l2`.

## Outcome

threadpool's `WorkPool` serves Phase 7c's step L2: merge aging per entry, a parse-ahead group, the reserve kept for queries, `block_guard` on non-worker waits, the architect's ruled wait (a wait serves only its own group, an own-group wait aborts), `on_idle`, and work conservation with one ticket per lane, with the K1 to K3 measurements rerun on the final code.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] L2-1 K1 to K3 reruns on the final code, data in `docs/status/phase7c-l2/` (16 workers, strict, 5 trials): `docs/status/phase7c-l2.md`
- A2 [artifact] L2-2 merge aging (D4, C8.2): `test/work_scheduling.cc` cases 1 to 4: `docs/status/phase7c-l2.md`
- A3 [artifact] L2-3 the other C8 items: parse-ahead group (case 5), reserve 1 (C8.3): `docs/status/phase7c-l2.md`
- A4 [artifact] L2-4 V2: a worker waiting on work queued only in its group (case 6; deadlock found and fixed): `docs/status/phase7c-l2.md`
- A5 [artifact] L2-5 V5: `Options::block_guard` on non-worker entry to `wait()` (case 7): `docs/status/phase7c-l2.md`
- A6 [artifact] L2-6 a mutant per claim (M1-M9), all die: `docs/status/phase7c-l2.md`
- A7 [artifact] L2-7 TSan and ASan+UBSan clean on all 6 CTest targets at threadpool:6573176: `docs/status/phase7c-l2.md`
- A8 [artifact] L2-8 consumers built against the branch: scheduler, logger, http-log, prism, strata 57/57, Xapiand configures and compiles: `docs/status/phase7c-l2.md`
- A9 [artifact] L2-9 `cpp-libraries/tools/preflight.sh` mac-hardened, mac-asan-ubsan, linux, freebsd ok: `docs/status/phase7c-l2.md`
- A10 [artifact] L2-10 CI green on the pushed branch: `docs/status/phase7c-l2.md`
- A11 [artifact] L2-11 review round B1, B3, B4 (and B2, N1 superseded), each with a mutant that dies: `docs/status/phase7c-l2.md`
- A12 [artifact] L2-12 the architect ruling AL1, AL2, AR2 implemented with `test/work_wait.cc` (I1-I6) and mutants: `docs/status/phase7c-l2.md`
- A13 [artifact] L2-13 K1 to K3 rerun on the ruled code, data in `docs/status/phase7c-l2/k-ruled/`: `docs/status/phase7c-l2.md`
- A14 [artifact] L2-14 re-check 2: B5, B6 and the two test gaps (I4b, AL2 loops), K1 to K3 in `docs/status/phase7c-l2/k-b6/`: `docs/status/phase7c-l2.md`
- A15 [artifact] scheduler.md K6 (FreeBSD kernel time against the old pool), carried by L2 (Next 4): `docs/status/phase7c-l2.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Supersedes `docs/status/scheduler.md` (feature/scheduler, SUPERSEDED 2026-10-29): R1 to R3 stand and the K evidence here is final; its K1-K5 and K7 are this quest's L2 rows.
- Architect ruling (`docs/reviews/phase7c-l2-code.md`): `wait()` serves only the group it waits on; own-group waits abort; rescue threads, `max_inline_depth` and `help_until` deleted.
- Decisions to confirm (architect), left open in the ledger: `block_guard` as a function pointer; per-entry aging; flushes do not age against queries; `reserve` default 1.
- Risk high: the shared pool every engine chore and query runs on (the auditor's mapping).

## May land early

None.
