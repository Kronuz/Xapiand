# faultlab-design: Design faultlab

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
is in `state.json`: `campaign status faultlab-design`.

## Outcome

`docs/design/faultlab.md` in cpp-libraries: the design of Kronuz/faultlab (D95, amended with
Detent D104), the one fault-injection library Xapiand, strata, journal, consensus and Detent
share. Input: `docs/research/faultlab-survey.md` (cpp-libraries:d36a131). The design fixes the
API before any code moves; the repo is created when it lands.

## Acceptance criteria

- A1 [artifact] `docs/design/faultlab.md` names faultfs's public API over `journal::IO`/`File`
  (completion/io.h): crash points (power_off_after, mutations, crash(seed)), sector tearing,
  namespace durability, ordering epochs, media/ENOSPC/short-write faults, and Detent's
  Entries x Data variants as a crash policy flag.
- A2 [artifact] The two seam additions (`File::preallocate`, `File::advise`) with signatures and
  defaults, and the sync question stated with the lead's default (one durable `sync`, strength set
  per process by completion's barrier policy) marked as pending the owner's ruling.
- A3 [artifact] The recording `IO` decorator that replaces Detent's sync hooks
  (test_relay_mirror_crash.cc), sized against the 475 lines it replaces.
- A4 [artifact] One seeded scenario format and one JSON-line event log; the Python history checker
  (Xapiand harness/cluster/history.py) as the one checker; netlab (netns + netem + nftables, with
  proxy.py as the portable tier) and disklab (dm-log-writes, dm-delay, dm-flakey) as optional
  Linux labs; what runs on macOS and FreeBSD.
- A5 [artifact] The migration order with sizes: strata (~60 tests), journal (-175 lines),
  consensus (4 tests), Detent (5 tests; Detent owns completion's sync seam after journal-driver),
  storage (43 call sites, CrashIO and test_crash.cc), Kronuz/io's random-error macros last. Each
  step leaves every repository green.
- A6 [artifact] Three questions for Detent's lead, answered or marked open in the doc.

## Decisions

- Owner (D95): one library, header-only crash model, portable; Linux labs optional.
- Lead: no new dependency beyond completion; no code moves in this quest.

## May land early

None.
