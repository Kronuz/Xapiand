# lib-doc-nits: Review nits in completion's barrier docs and header

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status lib-doc-nits`.

## Outcome

completion's barrier docs and header say what the code does (barrier-warning-completion review N1-N5): `sync_directory`'s flush target while an owner file is held, "warns again after a throw" stated exactly, the stale `barrier_unsupported` ENOTTY comment fixed, the first classification's cost documented (IOKit/CF/objc, 44-304 images; fork-without-exec hosts), and `barrier_volumes()` leaked so it has no exit-time destructor. N6 (Apple Silicon IOKit keys) is recorded as unmeasured.

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 The suite passes with `barrier_volumes()` leaked (no exit-time destructor): the build-test job
- A2 [artifact] `docs/mechanisms/barriers.md` carries N1, N2, N4, N5 and N6's statements: `docs/mechanisms/barriers.md`

## Contracts

None.

## Decisions

Lead: N7 (a throwing callback failing a commit) stays as it is, the owner's call.

## May land early

None.
