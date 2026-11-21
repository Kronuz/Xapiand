# Geo global prefixes: index and lookup order

Hunt row M3. A geo document indexed with a global prefix list was found only by the reversed list.

## Cause

`GenerateTerms::geo` (the overload with `acc_global_prefix`, src/multivalue/generate_terms.cc) fills
`level_terms[pos]` for `accuracy[last - pos]`. The field prefix is taken at `acc_prefix[last - pos]`; the
global prefix was taken at `acc_global_prefix[pos]`, so the global terms of each level sat under the prefix of
the mirrored accuracy. The lookup, `GenerateTerms::geo(ranges, accuracy, acc_prefix)`, pairs `accuracy[i]`
with `acc_prefix[i]`, the same as the field's own terms, so the same list found nothing. The integer, positive and datetime
overloads walk both lists in step and were not affected.

## Fix

One line: `acc_global_prefix[last_acc_pos - pos]` (src/multivalue/generate_terms.cc, the `gprefix` in the
indexing `geo`). The lookup is unchanged and stays symmetric with the field's own prefix; a lookup-side fix
would need the global list reversed only for global queries, a second rule for the same function.

The test in tests/test_coverage_multivalue_terms.cc (`test_geo_terms_and_query`) now states the agreement: the
list that indexed the document finds it and the reversed list does not (it pinned the opposite before).

## Reindex

Yes, for global geo terms: the indexed terms change (every level but a middle one of an odd-sized list moves
to another prefix). Global geo terms indexed before are under mirrored prefixes, which no query built from the
accuracy list reaches (the default list is the one the global spec uses), so existing indexes stay as unreachable
as they were until their documents are reindexed. The field's own prefixes do not change.

## Evidence

- Job 261121-042245-ffa941: the test at the test-only commit, source unfixed; expected to fail.
- Job 261121-042247-67ee57: the same test with the fix; expected to pass.
