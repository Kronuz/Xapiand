# newman-pin: Pin newman for the docs E2E in one place

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status newman-pin`.

## Outcome

One exact newman version, in one file under `harness/`, is what the docs E2E runs everywhere: `harness/engine_e2e.sh` and `harness/run_all.sh` install or resolve it themselves (a local, lockfile-pinned install or `npx --yes newman@X.Y.Z`), and CI's "Install newman" step (now `npm install -g newman@6`) uses the same pin or is removed in favor of the harness doing it. A newman found first on PATH no longer decides the version (e2e-harness A3, hunt row).

## Acceptance criteria

- A1 The docs E2E passes locally through the harness with a different newman (4.5) first on PATH: a job running `harness/engine_e2e.sh` that prints the pinned version it used
- A2 One place names the version: a job running `test $(git grep -n -E 'newman@?[0-9]' -- harness .github TESTING.md | wc -l) -le 1` (adjust to the chosen file, stated in the journal)
- A3 [ci] CI's Functional E2E (docs) is green, with the pinned newman

## Contracts

None.

## Decisions

Lead: pin an exact release (6.x current at the quest's start), with npm's lockfile when installed locally; no global install.

## May land early

None (one landing).

## Tasks

One maintenance contributor (Sonnet, medium).
