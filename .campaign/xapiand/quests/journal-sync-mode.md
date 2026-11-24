# journal-sync-mode: journal: the caller chooses the sync mode at open (plain fsync or full flush on macOS)

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status journal-sync-mode`.

## Outcome

A journal host (and every log under it) takes a sync mode, chosen explicitly when it is opened:
`full` flushes the drive cache (F_FULLFSYNC on macOS, today's only behavior) and `plain` uses the
platform's ordinary sync (fsync on macOS, which leaves data in the drive's cache). Nothing is
process-global. On Linux and FreeBSD both modes make the same call. Xapiand opens its journals in
`plain` by default and in `full` under `--full-flush-ordering`, the same as master's WAL (Xapiand
PLAN D177). Detent opens in `full`, so its durability claims are unchanged.

## Acceptance criteria

- A1 The mode is a required parameter at open, with no default and no global switch; every sync
  call the journal makes (file data, namespace barriers, the lock file) follows the host's mode:
  unit tests that open a host in each mode and count the calls each makes through the IO backend's
  test seam, by kind
- A2 The crash model runs in both modes. The faultfs model treats a `plain` sync as not
  surviving power loss on macOS, and a `full` sync as surviving it. The power-loss sweep shows
  `full` loses nothing acknowledged, and `plain` loses only what the model allows. A mutant that
  makes `full` issue a plain sync is killed by a named test (`--fails-without`).
- A3 In `full` mode an unsupported operation still fails instead of weakening, unchanged: the
  existing test passes, and a mutant that falls back to a plain sync in `full` mode is killed
- A4 [artifact] `README.md` and `ARCHITECTURE.md` state per mode what survives power loss on macOS
  ("plain fsync leaves data in the drive's cache"), and that "an unsupported operation fails
  instead of weakening" holds for `full`; `CHANGELOG.md` names the API change
- A5 Before and after on macOS at named commits: the journal bench's append rate in each mode
  against main (one `strict` job, numbers in `docs/mechanisms/`), stating the honest magnitude

## Contracts

Provides `journal:sync-mode`: the open-time mode in `include/journal/host.h` (or wherever the
host is opened). Changes journal's public API: every caller passes a mode. All callers are ours
(Detent, Xapiand); migrate them in their own quests, with no default that hides the choice.

## Decisions

- Owner, 2026-11-24 (Xapiand PLAN D177): the journal takes Xapiand master's macOS flush defaults.
- Detent's lead, 2026-11-24 (msg-321): xapiand owns this quest; Detent reviews the contract and
  its durability. Conditions: the mode is the caller's at open with no process-global switch;
  README and ARCHITECTURE state per mode what survives power loss; the crash-model tests run both
  modes and faultfs distinguishes them; ordered after `detent:journal-bound-first-open`, which
  holds `include/journal/host.h`.
- Lead: the separate finding that a lone voter makes two full flushes per proposal (hunt list) is
  not in this quest; it touches the turn log's commit protocol and gets its own quest.

## May land early

Nothing: the API change and the durability model land together.
