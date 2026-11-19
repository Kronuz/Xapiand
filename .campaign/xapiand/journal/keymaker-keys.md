# keymaker-keys: journal

State: waiting on jobs 261119-141751-bda861 261119-141757-1907bd 261119-140735-5f4129
Quest: .campaign/xapiand/quests/keymaker-keys.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

First jobs failed for real: Restored(unserialize_string(...)) took a string_view (compile error in metric_serialize.h, seen in the suite job's full log a1/log), and the test job used a wrong target name. Fixed at 32749f008 (std::string copy; target xapiand_test_coverage_multivalue); merged origin/master. Mutant commit b9d4c5314c9c (src reverted, tests kept). Jobs: test 261119-141751-bda861 (binary is build/bin/, first try 140732 built but could not find it), mutant 261119-141757-1907bd, suite 261119-140735-5f4129. Local syntax check could not run clean (stale deps in the scratch build), so the jobs are the check.

## Next

1. When jobs pass: State ready for review; paste `campaign brief` table.

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

- The M1 and M5 fixes are in src/multivalue/keymaker.h (the constant and StringKey live there). The M4 fix is in src/metric_serialize.h (SerializablePhoneticMetric), outside the file set src/multivalue/keymaker.*; it is 7 lines, the only home of that type. Lead: widen the file set or accept.
- M4: the encoder's _code_str is restored through a local Restored subclass (the soundex and dmetaphone constructors re-encode, so building from the code would change it).
- Changelog line for the lead: "Multivalue sort keys: a descending string sort on a missing slot keys as the minimum (one NUL byte, not empty); a stored string sort key keeps its slot and direction and a stored soundex or dmetaphone key keeps its encoder code across serialize and unserialize. Stored sorters written before this hold no slot or direction for string keys and no longer unserialize."
