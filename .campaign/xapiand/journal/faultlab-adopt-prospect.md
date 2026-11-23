# faultlab-adopt-prospect: journal

State: working
Quest: .campaign/xapiand/quests/faultlab-adopt-prospect.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

<The current step, precisely enough for a fresh contributor to continue it.>

## Next

1. <step>

## Blocked on

<What, and who decides. "Nothing" when nothing.>

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

## Contributor update
State: waiting on jobs build-test 261120-160356-152fa6, A2 261120-160358-45a507, counts at branch 261120-160400-a4d76b, counts at main 261120-160403-a0ffdf. Branch pushed (d4a4022 + AGENTS.md revert).
Done: 17 tests + tools/parity_terms.cc include faultlab/faultfs.h, `namespace faultlab = kronuz::faultlab`; faultfs:: -> faultlab::. CMake: faultlab pinned b64102f, completion pinned d8b538c ahead of strata's (faultlab needs its seam), STRATA_FAULTFS_MAP -> FAULTLAB_MAP.
Next: diff the two counts outputs (must be identical; a moved seed is a finding).
Rulings I made: test/phrase_expansion_fixture.hh (outside file set) changed, A2 needs it; AGENTS.md line on the env var left stale (outside file set). faultlab linked on the prospect interface target.
Blocked on: lead to add the fixture and AGENTS.md to the file set.

## Lead rulings applied (new head, pushed)
Lead decisions: completion pin d8b538c kept (descends from strata's 7731315); fixture and AGENTS.md in the file set; faultlab not on prospect's interface target.
Done: tests/tools link prospect::testing (prospect + faultlab); ctest `interface_clean` fails if faultlab is in INTERFACE_LINK_LIBRARIES; AGENTS.md names FAULTLAB_MAP.
State: waiting on jobs build-test 261120-160718-9b0668, counts at branch 261120-160718-d41d00, A2 261120-160720-9ea5ec, interface check 261120-160721-a2e94d. Main counts stay 261120-160403-a0ffdf.
Next: diff branch counts against main counts.
