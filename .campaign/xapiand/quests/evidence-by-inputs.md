# evidence-by-inputs: Reuse evidence while a job's declared inputs are unchanged; land merges main itself when disjoint

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status evidence-by-inputs`.

## Outcome

Evidence survives changes that cannot affect it, and a finished quest lands without a second CI round when main moved elsewhere. On 2026-11-19 document-gate queued 23 exclusive reruns (~4 h of machine time alone) because one bench script gained a new set (tenx1), and four green quests were refused at land because main changed only tests and a harness outside their file sets, each costing a merge, a full CI round and, for high risk, a re-accept (~1 h each).

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 `campaign job submit --inputs GLOBS` records the paths a job reads; `land`, `brief` and `verify` count it as evidence at a later commit when no file matching its inputs (plus the build files the job's level implies, at most a documented default set) changed, and as stale otherwise: tests, plus a fails-without job at main
- A2 A job without --inputs keeps today's identical-code rule: test
- A3 When main changed only files outside a quest's set and outside every file its accepted reviews saw, `land` merges main itself, requires CI green on that merge commit (pushing it and waiting, or refusing with the exact command to wait), and keeps the acceptances; otherwise it refuses as today: tests
- A4 The suite passes, and SKILL.md and references/host.md describe both rules in place of the old text (each touched file ends shorter or equal in words): the suite job

## Contracts

Job specs gain an optional `inputs` list; old specs read as before.

## Decisions

- The lead maintains the campaign skill (owner, 2026-11-17).
- Never reuse a job across a change to its declared inputs; the default set covers the build files (CMakeLists.txt, toolchain pins) so a build change always reruns.

## May land early

None.
