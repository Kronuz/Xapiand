# keymaker-keys: journal

State: waiting on jobs 261119-123351-6084a3 261119-123354-0d438a 261119-123359-293e2f
Quest: .campaign/xapiand/quests/keymaker-keys.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fixes committed at 792807431 (keymaker.h MIN_STR_CMPVALUE and StringKey slot/direction; metric_serialize.h phonetic code restore). Pins flipped in tests/test_coverage_multivalue.cc. Mutant commit bd701f5b2e00 (src reverted, tests kept; ref refs/mutant/keymaker-keys). Jobs: test 261119-123351-6084a3, mutant 261119-123354-0d438a, suite 261119-123359-293e2f.

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
