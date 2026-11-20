# keymaker-keys: journal

State: ready for review
Quest: .campaign/xapiand/quests/keymaker-keys.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Done; all jobs passed at 78653b157 (journal-only commits since).

## Next

1. Lead reviews; push the branch for CI (A4 also needs CI green).

## Blocked on

<What, and who decides. "

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261119-165353-c9efd0; fails without: 261119-123354-0d438a, 261119-165358-64761d |
| A2 | 261119-165353-c9efd0; fails without: 261119-123354-0d438a, 261119-165358-64761d |
| A3 | 261119-165353-c9efd0; fails without: 261119-165358-64761d |
| A4 | 261119-165403-4deb85 (suite passed); CI not run, branch not pushed |

## Result

Sort keys keep what they encode: MIN_STR_CMPVALUE is one NUL byte (M1), StringKey serializes its slot and direction (M5), a phonetic key restores its encoder code so soundex english round-trips at 13 bytes (M4), and a Jaccard key restores its character set (found by the round-trip test; slot 12 rebuilt to a different key). Pins in tests/test_coverage_multivalue.cc flipped; the mutant (fixes reverted) fails on the new CHECKs. No docs change beyond the changelog line below.

## Rulings I made

- The M1 and M5 fixes are in src/multivalue/keymaker.h (the constant and StringKey live there). The M4 fix is in src/metric_serialize.h (SerializablePhoneticMetric), outside the file set src/multivalue/keymaker.*; it is 7 lines, the only home of that type. Lead ruled: it is already in the file set; no widening needed.
- Jaccard (found while flipping the pins): SerializableMetric::unserialize rebuilds the metric from the string and icase so derived state is restored; changelog line gains "and a stored Jaccard key keeps its character set".
- M4: the encoder's _code_str is restored through a local Restored subclass (the soundex and dmetaphone constructors re-encode, so building from the code would change it).
- Changelog line for the lead: "Multivalue sort keys: a descending string sort on a missing slot keys as the minimum (one NUL byte, not empty); a stored string sort key keeps its slot and direction and a stored soundex or dmetaphone key keeps its encoder code across serialize and unserialize. Stored sorters written before this hold no slot or direction for string keys and no longer unserialize."
