# keymaker-keys: journal

State: ready to land (lead filled Evidence from the finished jobs, 2026-11-19 21h).
Quest: .campaign/xapiand/quests/keymaker-keys.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Review F1 (CHANGES): M1 was not a fix; the empty MIN_STR_CMPVALUE already keyed a missing slot last in descending order. Restored the empty constant (fb0704f18), pinned "" at tests/test_coverage_multivalue.cc, and added a descending check: list("","\0") sorts ahead of a missing slot, list("","") ties with it (the empty constant's order). Jobs at fb0704f18: test 018efe, mutant c8b5f6 (ac35d6847bda: the one-NUL constant back, fails on the new check), suite 0d34b1. F3 (all-kinds icase x direction loop) not added; the reviewer's probe passed 96/96 and the optional loop is left out.

## Next\n\n1. When jobs pass: fill Evidence from `campaign brief`, State ready for review.\n\n## Blocked on

<What, and who decides. "

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261119-200941-018efe; fails without (one-NUL constant): 261119-200950-c8b5f6 |
| A2 | 261119-165353-c9efd0 (at 78653b157); fails without: 261119-165358-64761d; rerun: 261119-200941-018efe |
| A3 | 261119-165353-c9efd0; fails without: 261119-165358-64761d; rerun: 261119-200941-018efe |
| A4 | 261119-200952-0d34b1 |

## Result

Sort keys keep what they encode: MIN_STR_CMPVALUE stays the empty string (M1 was no defect: the empty string already keyed a missing slot last in descending order, and a one-NUL constant misorders list("","") and list("",NUL); the pin is now "" and a descending check orders them), StringKey serializes its slot and direction (M5), a phonetic key restores its encoder code so soundex english round-trips at 13 bytes (M4), and a Jaccard key restores its character set (found by the round-trip test; slot 12 rebuilt to a different key). Pins in tests/test_coverage_multivalue.cc flipped; the mutant (fixes reverted) fails on the new CHECKs. No docs change beyond the changelog line below.

## Rulings I made

- F1 (lead's ruling, the reviewer's recommendation): M1 reverted to the empty constant; the hunt-row M1 wording is the lead's to fix in hunt.md. A1's earlier claim is withdrawn.

- The M1 and M5 fixes are in src/multivalue/keymaker.h (the constant and StringKey live there). The M4 fix is in src/metric_serialize.h (SerializablePhoneticMetric), outside the file set src/multivalue/keymaker.*; it is 7 lines, the only home of that type. Lead ruled: it is already in the file set; no widening needed.
- Jaccard (found while flipping the pins): SerializableMetric::unserialize rebuilds the metric from the string and icase so derived state is restored; changelog line gains "and a stored Jaccard key keeps its character set".
- M4: the encoder's _code_str is restored through a local Restored subclass (the soundex and dmetaphone constructors re-encode, so building from the code would change it).
- Changelog line for the lead: "Multivalue sort keys: a stored string sort key keeps its slot and direction and a stored soundex or dmetaphone key keeps its encoder code across serialize and unserialize. Stored sorters written before this hold no slot or direction for string keys and no longer unserialize."
