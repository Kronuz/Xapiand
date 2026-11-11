# x7-coverage: journal

State: waiting on jobs 261111-144242-730675 261111-144244-01ce7e 261111-144409-ecb5b2
Quest: .campaign/xapiand/quests/x7-coverage.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Written and committed: harness/restore_corpora.sh (+_gen.py), tests/corpora/source.ndjson, harness/coverage.sh, TESTING.md (root; targets copied from strata/docs/TESTING.md, which the quest's 'TESTING.md' names; none existed in this repo).
Jobs: alpha9 build+corpus (730675, slot x7-old), master build+corpus+replay smoke (01ce7e, slot x7-new), coverage (ecb5b2, slot x7-cov, WORKLOADS=ctest e2e).

## Next

1. Copy the corpora from the jobs' $JOB_OUT (alpha9/, master/) into tests/corpora/<name>/ (rt.mp, BUILD), write tests/corpora/README.md naming the builds, commit.
2. Submit the A2 replay job (`harness/restore_corpora.sh check`) at that commit; fix any restore difference (a real one goes to Blocked on).
3. Coverage job result: paste the table in the journal and TESTING.md-adjacent note; rerun coverage with corpora workload if wanted.

## Blocked on

<What, and who decides. "

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
