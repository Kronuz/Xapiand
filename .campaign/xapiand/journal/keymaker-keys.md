# keymaker-keys: journal

State: waiting on jobs 261119-165353-c9efd0 261119-165358-64761d 261119-165403-4deb85
Quest: .campaign/xapiand/quests/keymaker-keys.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Merged origin/master. Cause of the lines 481/504 failures found: slot 12 is Jaccard, which derives a character set from its string in the constructor; unserialize set only the string, so a rebuilt Jaccard key had an empty set (a real bug, fixed in src/metric_serialize.h by rebuilding the metric in SerializableMetric::unserialize). Line 248 was dropped earlier. Resubmitted at 78653b157: test (c9efd0), mutant (64761d, commit eb6441038134 = head with the src fixes reverted, ref refs/mutants/keymaker-keys; builds first, exits 127 on build failure, fails only on a FAIL line) and suite (4deb85).

## Next

1. When jobs pass: fill the Evidence table from `campaign brief`; State ready for review.

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

- The M1 and M5 fixes are in src/multivalue/keymaker.h (the constant and StringKey live there). The M4 fix is in src/metric_serialize.h (SerializablePhoneticMetric), outside the file set src/multivalue/keymaker.*; it is 7 lines, the only home of that type. Lead ruled: it is already in the file set; no widening needed.
- Jaccard (found while flipping the pins): SerializableMetric::unserialize rebuilds the metric from the string and icase so derived state is restored; changelog line gains "and a stored Jaccard key keeps its character set".
- M4: the encoder's _code_str is restored through a local Restored subclass (the soundex and dmetaphone constructors re-encode, so building from the code would change it).
- Changelog line for the lead: "Multivalue sort keys: a descending string sort on a missing slot keys as the minimum (one NUL byte, not empty); a stored string sort key keeps its slot and direction and a stored soundex or dmetaphone key keeps its encoder code across serialize and unserialize. Stored sorters written before this hold no slot or direction for string keys and no longer unserialize."
