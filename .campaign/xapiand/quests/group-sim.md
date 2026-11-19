# group-sim: Phase 3R gate rows in the simulator

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status group-sim`.

## Outcome

strata's deterministic simulator, ported from the unmerged `design/simulator` prototype (`sim/`:
engine, network, history, checker; 961 commits behind main) onto `main`, drives `strata::group`
(group-core, group-install) and passes the Phase 3R rows on the release's scope (design section 5,
quest 7; phase3r A2-A4). The port starts now against group-core's quest-file contract; the rows run
once group-install lands.

## Acceptance criteria

- A1 At least 10^7 steps over at least 1,000 seeds, for 3 and 5 voters, with crashes, power loss at
  every barrier with torn and reordered writes, partitions, clock skew, add, remove, replace, D7's
  0-to-1 growth window, chunked commands cut by a leader change, and corruption of every file class:
  zero `quorum`-acknowledged loss, zero served corrupt bytes, and a rebuild after a disk loss
  (simulator jobs, their seeds and step counts stated)
- A2 The linearizability checker passes over write histories and `linearizable` get-by-id
  histories, and checks `prefix` get-by-id for consistent prefix only (design D4; a job)
- A3 Mutants killed, each a `--fails-without` job: an apply that skips one committed entry; a
  recovery that ignores the checkpoint's file-set lengths; a `read_applied` without the
  quorum-probe; a leader that applies an abandoned chunk of a lower term
- A4 The simulator injects a device that persisted a non-prefix subset of the writes since its
  last flush (what D14's replay cannot give), and the journal's checksums and C3 repair answer it:
  a test with values
- A5 [artifact] `docs/gates/phase-3r.md`: the rows, their jobs and commits; and
  `docs/mechanisms/simulator.md`

## Contracts

Consumes `strata:group` and `strata:group-install` through their public C++ API only; the
simulator owns its fault-injecting `HeldCompletion` and network. Provides nothing on disk or wire.

## Decisions

- Design section 5: phase3r A1-A4 and A7 become group-core, group-install and this quest; phase3r
  A5, A6 and A8 follow the release. The lead narrows `phase3r` (it holds
  `docs/gates/phase-3r.md`) and `track-d` (it holds `sim/**`) before this quest starts.
- Lead: the simulator's tests live under `sim/`, built by `sim/CMakeLists.txt`, which strata's root
  adds when present (the prerequisite quick fix), so this quest needs no root `CMakeLists.txt`.

## May land early

The port of `sim/` with its own tests and no gate rows.
