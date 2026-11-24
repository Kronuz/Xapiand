# journal-sync-mode: journal: the caller chooses the sync mode at open (plain fsync or full flush on macOS)

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status journal-sync-mode`.

## Outcome

The journal passes completion's flush mode (`completion-flush-mode`) through from whoever opens
it, explicitly, with no default and no process-global switch, and makes no sync of its own. A host
opened `plain` makes plain syncs everywhere, the turn barrier included. Xapiand opens `plain` by
default and `full` under `--full-flush-ordering`, the same as master's WAL (Xapiand PLAN D177).
Detent opens `full`, so its durability claims are unchanged. The crash model runs in both modes,
with faultfs's plain-sync model (`faultlab-plain-sync`).

## Acceptance criteria

- A1 Journal pins completion and faultlab at their landed commits; every PosixIO the journal's own
  tests, benches, examples and fuzzers build passes a mode explicitly. A test opens a host in each
  mode and asserts, through completion's IO seam, that the turn barrier's and the checkpoint's syncs
  follow it.
- A2 The crash model runs in both modes. The power-loss sweep shows `full` loses nothing
  acknowledged, and `plain` loses only what faultfs's plain model allows. A mutant that opens `full`
  as `plain` is killed by a named test (`--fails-without`).
- A3 In `full` mode an unsupported operation still fails instead of weakening: the existing test
  passes at the new pins.
- A4 [artifact] `README.md` and `ARCHITECTURE.md` state per mode what survives power loss on macOS
  ("plain fsync leaves data in the drive's cache"), and that "an unsupported operation fails
  instead of weakening" holds for `full`. `CHANGELOG.md` names the API change.
- A6 One table, checked against the real calls: a test records the calls completion's PosixIO makes
  per operation and mode (completion's `replace_sync_calls` seam) and compares them with faultlab's
  `call_for`/`persists`, so a call completion weakens cannot stay "persists" in the model (review of
  faultlab-plain-sync, Q3). If completion's `FlushMode` moves to io.h, faultlab uses it and drops its
  own enum.
- A5 Before and after on macOS at named commits: the journal bench's append rate in each mode
  against main (one `strict` job, numbers in `docs/mechanisms/`), stating the honest magnitude.

## Contracts

Consumes `completion:flush-mode` and `faultlab:plain-sync`; provides `journal:sync-mode`, the
mode at open. All callers are ours: Detent and Xapiand migrate in their own quests.

## Decisions

- Owner, 2026-11-24 (Xapiand PLAN D177): the journal takes Xapiand master's macOS flush defaults.
- Detent's lead, 2026-11-24 (msg-321): xapiand owns this quest; Detent reviews the contract and
  its durability. Conditions: the mode is the caller's at open with no process-global switch;
  README and ARCHITECTURE state per mode what survives power loss; the crash-model tests run both
  modes and faultfs distinguishes them; ordered after `detent:journal-bound-first-open`, which
  holds `include/journal/host.h`.
- Lead, 2026-11-24: re-scoped after the contributor found the journal issues no sync itself:
  every call is completion's PosixIO, and the crash model is faultlab's. The mode lives in
  `completion-flush-mode`; faultfs's model in `faultlab-plain-sync`; this quest wires them, runs
  the sweep in both modes, and documents and measures them.
- Detent's lead (msg-325): Detent opens journal hosts and never builds a PosixIOOptions, so the mode
  is a required parameter where a host is opened (host.h), with no default; this quest lands after
  detent:journal-bound-first-open, and the lead sends Detent that call's exact shape before landing.
- Lead: the separate finding that a lone voter makes two full flushes per proposal (hunt list) is
  not in this quest; it touches the turn log's commit protocol and gets its own quest.

- Lead, 2026-11-24 (session 2's finding): the mode lives where completion put it, in
  `PosixIOOptions`, and a Host follows its IO; host.h does not change. msg-325's premise that Detent
  never builds a PosixIO is false: Detent builds them in src/detent_authority/service.cc:1835-1851,
  src/detent_local/disk_store.cc:467, src/detent_relay/mirror_service.cc:364, benchmarks and
  consensus's bench/netlab/node.cc:211. One mechanism, one place: a second mode at host open could
  disagree with its IO's. Detent's lead is told with the call shape before landing.

## May land early

Nothing: the API change and the durability model land together.

## Review

One `durability` review, strong tier at effort max, on Detent's behalf (msg-323); Detent's lead reads it
for its acceptance. Its questions:
1. Can any path reach a plain sync while the caller asked for full: defaults, reopen, the host's
   sync_files, the artifact or mutation phases, directory barriers, the turn barrier's direct
   `sync_data` (read with completion-flush-mode's review)?
2. Does the full-mode refusal path still fail and never degrade, shown by a run?
3. Does faultfs drop the drive-cache writes in plain mode at a power cut, and does a mutant that maps
   full to plain fail the crash model?
4. Is the README's per-mode claim true on Linux and FreeBSD, where plain already makes the full call?
