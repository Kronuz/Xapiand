# faultlab-disklab: journal

State: ready for review
Quest: .campaign/xapiand/quests/faultlab-disklab.md (the lead's; never edit it)

## Doing

Nothing: every criterion has its job at faultlab:131aa45 (A1/A2 at faultlab:d534091; the later
commit is docs only).

## Next

1. Review; CI after the lead registers the test and pushes (Blocked on).

## Blocked on

The lead, for CI (A4) and two files outside the file set:
- CMakeLists.txt: add `disklab_test` to `foreach(t selftest proxy_test scenario_test netlab_test)`.
  Off Linux-root it runs the synthetic-log and recording-lab halves and prints
  `SKIP live probes and teardown: <reason>`; `--live` makes the skip a failure.
- README.md: add `faultlab/disklab.py    Linux dm-log-writes, dm-delay, dm-flakey` to the
  layout block beside netlab.py, and `disklab` to the `from faultlab import ...` line.
- CI on the quest branch needs a push, which contributors do not do.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261118-072844-9a21c4 (at faultlab:d534091; later commit docs only) |
| A2 | 261118-072844-9a21c4 (at faultlab:d534091; later commit docs only) |
| A3 | 261118-073106-54b543; fails without: 261118-073108-b7211a (mutant faultlab:f82d919, `heal` writes no event; ref refs/mutants/faultlab-disklab-a3) |
| A4 | 261118-073109-9a83d0; CI: see Blocked on |

Live run (Ubuntu 24.04, kernel 6.8, 4 vCPU VM on 18782, images on /dev/shm):
- A1: points from mark `before` to mark `after`: #98 mark before (planted missing), #106 flush
  (missing), #107 FUA 4 KiB, the jbd2 commit block (present), #108 mark after (present); the
  synced file present at every point. Seconds per replay (rebuild plus recovering mount): median
  0.055 s, max 0.114 s over a 32.5 MiB log of 127 entries; 0.26 s over a 314 MiB log of 2,833
  entries (256 fsynced 1 MiB files on a 1 GiB ext4), rebuild alone 0.21 s (1.5 GB/s). The 20-run
  power-loss tier pays under a second per node per run for replay.
- A2: delay(100) adds 104.0 ms to fdatasync after an O_DIRECT overwrite (one flush; median 0.28
  -> 104.28 ms, healed 0.28); a buffered write plus fsync gains 414 ms (4.1 device trips).
  media('error'): (EIO, landed) of 20 O_DIRECT writes before (0, 20), inside (20, 0), after
  (0, 20); media('drop'): before (0, 20), inside (0, 0), after (0, 20).
- Teardown: clean after close, exit, SIGTERM, SIGHUP, SIGINT; SIGKILL (also with a replay
  mounted) leaves its pieces, which `disklab.py sweep` and the next lab clear; listing after
  equals before.

## Result

faultlab `python/faultlab/disklab.py`: filesystem -> fault layer (`<name>-disk`: linear, delay
or flakey, swapped by suspend --nolockfs / load / resume) -> dm-log-writes (`<name>-log`) -> two
loop devices on sparse images. `open`, `mark`, `delay(ms)`, `media(mode)`, `heal`, `stop`,
`points`, `replay(to)`, `teardown`/`close`, `sweep`; `read_log`, `find`, `rebuild` replay a log
in pure Python. Docs: `docs/api.md` (disklab.py), `docs/mechanisms/disklab.md`. Test:
`python/tests/disklab_test.py [--live] [--root DIR]`. VM jobs run through
`~/.scratch/faultlab-disklab/vmjob.sh` (starts the 18782 VM only when stopped, stops only what it
started). Leaves open: `enospc` (F12 kind; not in this quest), and `tools/vm/**` unused.

## Rulings I made

- Event kinds: `delay` (`ms`), `media` (`mode`: error|drop), `heal`, `power_cut` (`at`: mark name
  or entry index), each with `nodes=[node]` when the lab has one. `delay` and `heal` reuse
  network kind names (F12 lists only power_cut, media, enospc for disk); the absence of network
  `nodes` pairs and the lab's node tell them apart. The lead may want F12 to list them.
- `power_cut` is written at `replay`, when the cut state is chosen, not during the run.
- One fault layer, above dm-log-writes: dropped or failed writes never reach the log, so replay
  matches the device the filesystem saw. One fault at a time; `delay` then `media` replaces.
- Table swaps use `dmsetup suspend --nolockfs`: a default suspend freezes (syncs) the filesystem,
  adding writes and, inside a media window, aborting the journal.
- `media` window is explicit (`media` ... `heal`), flakey up 0 s / down 1 s, rather than
  flakey's cyclic timer: deterministic edges.
- Replay is Python over the log (no xfstests replay-log dependency), rebuilt from zero per
  replay: 0.05-0.26 s measured, so no snapshot machinery.
- Replay needs `stop` first: the target writes its final super and end mark at removal; `stop`
  raises when the end mark is missing (log full).
- mkfs.ext4 default `-E nodiscard,lazy_itable_init=0,lazy_journal_init=0`: no background
  zeroing thread writing into the log after mount.
- Reuses netlab's `FaultLog`, `Runner`, `run`; `DisklabError` subclasses `NetlabError` so one
  `except` catches both labs' command failures. The atexit/signal trap (about 20 lines) is
  duplicated from netlab, as netlab.py is outside the file set; a shared `faultlab/lab.py` would
  hold one copy (lead's call).
- The live test keeps images on `/dev/shm` (`--root`) so loop flushes cost nothing and fsync
  timing measures dm-delay alone; the library default root is `/var/tmp`.
