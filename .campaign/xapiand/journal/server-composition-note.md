# server-composition-note: journal

State: ready for review (owner rulings folded at cpp-libraries:0579a1a)
Quest: .campaign/xapiand/quests/server-composition-note.md (the lead's; never edit it)

## Doing

Nothing: the note is committed at cpp-libraries:0579a1a on quest/xapiand/server-composition-note
(worktree cpp-libraries-wt/xapiand/server-composition-note), on top of the review fold at
cpp-libraries:21b3d33. The lead pushes and sends A5.

## Next

1. Lead: A5 with Detent's lead (F1, F4, F6, the detent-agent column, the relay daemonizing point
   below), then land.

## Blocked on

Nothing. For A5: Detent's F4 text said detent-relay does not daemonize through detent_cli, but
Detent:372e95c7 `src/detent_relay/main.cc:62-68` calls `detent_cli::run_daemon` and `ProcessLock`
as detentd does (`src/detentd/main.cc:154-158`); the note follows the code.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/server-composition.md` at cpp-libraries:0579a1a. F1: "Options" paragraph and C9 (--upstream-ca). F2: step 4's descriptor check, C5 (D133: explicit value exits EX_CONFIG, default shrinks). F3: step 4's Limits and Tls, C1. F4: step 2, C7. F5: step 6, C6. F6: step 8, C8, Deviations row. F7, F10-F14 also folded. |
| A2 | `docs/server-composition.md` at cpp-libraries:0579a1a: every cite re-read at task:95798a0, http:ec273dd, reactor:bf874dc, Detent:372e95c7, prism:4d3c96d, Xapiand:eb3dcfa80; detent-agent cites (`detent_agent/main.cc:105-116`, `config.hh:9`, `managed_host.cc:28-112,186-190,198-228`, `detent_delivery/coordinator.cc:253,316,446`) read at Detent:372e95c7; the cited Detent sources have no diff 372e95c7..d3141878. |
| A3 | `docs/server-composition.md` at cpp-libraries:0579a1a, section Deviations: 18 rows, each with reason and status; adds detent-agent's 1 ms and 20 ms polls, its `signal_set`, and the unbounded stops (detent-authority, relay, agent). |
| A4 | `docs/server-composition.md` at cpp-libraries:0579a1a, lines 1-4: "Status: CURRENT (2026-11-19), reviewed", naming Xapiand D132, D133, D138 and Detent D122, D123 and the six heads the citations hold at. CURRENT at cpp-libraries:0579a1a. |
| A5 | Manual, the lead sends it. Source of the Detent text so far: Detent lead cmux 3708c307 (F1, F4, F6). |
| doc check | job 261119-051022-2f4115 exit 0 at cpp-libraries:21b3d33; job 261119-063111-a8dbca exit 0 at cpp-libraries:0579a1a |

## Result

`cpp-libraries/docs/server-composition.md` (cpp-libraries:0579a1a) is the composition every Kronuz
daemon adopts (Xapiand, Prism, detent-authority, detent-relay, detent-agent): eight steps, rules
C1-C10, Options and Mains tables at the pinned heads, a Deviations table, and an appendix for
Xapiand's adoption. Docs only.

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

- D132: the driver is `task::lifecycle(loop, stop, {}, ready, components...)`; `stop` is a stop
  source whose type task names; C10 records it. Dump and restore now run the Registry under
  `lifecycle` with the tool's end as stop source (D132 names "a tool's end"); before, "without
  `serve`". `served` dropped from the appendix (task names the result).
- D138: the detent-agent Process cell is filled (`run_daemon`, `ProcessLock`, main.cc:105-116), not
  "no listener" as D138's wording reads: the agent runs that code, and "no listener" there would
  say it skips the C7 step. Its `ready()` call sits in the Housekeeping cell (the 20 ms monitor),
  so the `lifecycle`, ready row reads "no listener". Adoption order: Xapiand first, then Detent's
  daemons and Prism (Detent D122), replacing "Prism first".
- D123 (lead msg, mid-task): detentd is detent-authority in prose and headers; `detentd/` paths
  stay as at Detent:372e95c7; one line names D123.
