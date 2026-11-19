# server-composition-note: journal

State: ready for review
Quest: .campaign/xapiand/quests/server-composition-note.md (the lead's; never edit it)

## Doing

Nothing: the note is committed at cpp-libraries:21b3d33 on quest/xapiand/server-composition-note
(worktree cpp-libraries-wt/xapiand/server-composition-note). The lead pushes.

## Next

1. Lead: confirm A5 with Detent's lead on the one point below (relay's daemonizing), then land.

## Blocked on

Nothing. One point for Detent's lead (A5): their F4 text says detent-relay does not daemonize
through detent_cli, but Detent:372e95c7 `src/detent_relay/main.cc:62-68` calls
`detent_cli::run_daemon` and `ProcessLock` exactly as detentd does (`src/detentd/main.cc:154-158`).
The note follows the code (relay: "the same"), so no relay deviation is listed. If they meant
something else (the relay's readiness comes from `turn`, `host.cc:367-377`), it is a value, not a
path.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/server-composition.md` at cpp-libraries:21b3d33. F1: "Options" paragraph and C9 (`--upstream-ca`). F2: step 4's descriptor check, C5. F3: step 4's `Limits`/`Tls`, C1. F4: step 2, C7. F5: step 6, C6. F6: step 8, C8, Deviations row. F7, F10-F14 also folded (Options, C2, step 3, ledger-prefixed IDs, Settled 1 and 5). |
| A2 | Same file: every cite re-read at task:95798a0, http:ec273dd, reactor:bf874dc, Detent:372e95c7, prism:4d3c96d, Xapiand:eb3dcfa80 (`git -C ... show`); Mains Housekeeping row names the 5 s `control::Periodic` retry (manager.cc:184,706-709); Sizing 8.6 KB / 40 KB, 41 MB per 1024 TLS (http docs/perf/idle.md:56,75-82); relay mirror in Mains "Loop and services" and the `--upstream-ca` row (host.cc:155-296, configuration.cc:101-103,151,157); C2 corrected. |
| A3 | "Deviations" table: 17 rows, each with reason and status (removed by which decision, or stays and why). |
| A4 | Status line: "CURRENT (2026-11-19), reviewed", naming the six heads. |
| A5 | Source: Detent lead cmux 3708c307 (wording for F1, F4, F6, relayed by the lead; no message sent). F1 used with the `--upstream-ca` rename (Xapiand and Prism have no CA option: opts.cc:199-201, prism main.cc:246,258-259). F6 used as C8. F4 used except the relay point under Blocked on. |
| doc check | job 261119-051022-2f4115, exit 0 at cpp-libraries:21b3d33 |

## Result

`cpp-libraries/docs/server-composition.md` (cpp-libraries:21b3d33) is the composition every Kronuz
server adopts: eight steps, rules C1-C9, Options and Mains tables at today's heads, a Deviations
table, and an appendix "Xapiand's adoption" (the review's manager.cc map with destinations updated
to the rulings, and the five-step order) for Xapiand's adoption quest. Docs only; no code.

## Rulings I made

- F2: the descriptor check is `reactor::raise_open_files(wanted)`, called by `main` for plain and
  TLS, before `demote`; it raises the soft limit toward the hard one and returns the limit. The
  reserve is an app value: 64 (today's TLS spare) plus app pools (Xapiand 50 + 7 per index).
  F2b: an explicit `--max-clients` that does not fit refuses `EX_CONFIG`; the default shrinks with
  a warning (review option c). This narrows Xapiand's startup for an explicit too-large value
  (today it shrinks, main.cc:457-465): owner may overturn.
- F3: `Limits` holds every bound and absorbs `AsioResponseOptions`' five limit fields (it keeps
  only transforms); the constructor takes `http::Tls {context, failed}` and builds the TLS session
  from `Limits`, so `tls_connection_factory` and `TlsAdmission`'s count leave the public surface.
- F4/C7: Detent's `run_daemon`/`ProcessLock`/`locked_pid` is the one implementation and moves to
  task beside `stop_signals` (a D73 revisit for this step only; the lead may want it recorded).
  Signals moved to step 3, after the fork (its thread must not cross one). Data-directory
  exclusion belongs to the state service that writes the directory; Xapiand's registry gains a
  `flock`.
- F5/C6: state is constructed in `main` and passed by reference; a process global is a deviation.
  Measured scope: 71 `XapiandManager::` uses outside manager.cc/.h, 214 lines naming the manager
  (96 in test_node.cc), `git grep -w` at Xapiand:eb3dcfa80; the review's "about 190" did not
  reproduce with any pattern I tried, so the note gives these counts.
- F6/C8: no fixed-interval polling, per Detent's text.
- F10/C2: `server_context` sets `strict` for all apps; Detent's reconnect cost is listed as
  unchecked, with D62 as the escape.
- F11: the dump runs from the signal thread; service-shape (DRAFT) still says `signal_set`, noted
  under Limits for task's `stop_signals` quest.
- F7: Xapiand adds `--reactors` defaulting from `--processors`.
- The 1.000 descriptors/connection job (261112-101615-0bdb50) is not in either job store; marked
  unchecked in the note.
