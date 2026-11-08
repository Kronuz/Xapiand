# prospect-host-collectors: Search over several indexes (P4)

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at prospect:d8274b7 (2026-10-27); its ledger `docs/status/host-collectors.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-host-collectors`.

## Outcome

`shards.hh` searches several indexes as one: a docid map interleaved as Xapian does, `multi_snapshot`, pruned merge, exhaustive search with host key and visitor, and document-length bounds (xapiand-on-strata P4; G5, G7, G11).

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] H1 identity with one merged index, 50,400 comparisons, bit for bit: `docs/status/host-collectors.md`
- A2 [artifact] H2 docid interleave matches Xapian's: 3,696 oracle rows (`test/fixtures/xapian-shards`): `docs/status/host-collectors.md`
- A3 [artifact] H3 property test over random shard splits, 20,000 round trips: `docs/status/host-collectors.md`
- A4 [artifact] H4 the exhaustive visitor sees every match once (145,089 matches): `docs/status/host-collectors.md`
- A5 [artifact] H5 33 mutants, each caught: `docs/status/host-collectors.md`
- A6 [artifact] H6 ASan+UBSan and TSan clean (four threads on one snapshot): `docs/status/host-collectors.md`
- A7 [artifact] H7 CI run 40629357689 green on 8 jobs (before the rebase): `docs/status/host-collectors.md`
- A8 [artifact] H8 `docs/mechanisms/multi-index-search.md`: `docs/status/host-collectors.md`
- A9 [artifact] H9 smoke A/B of single-index search: identical results, noise: `docs/status/host-collectors.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- The oracle side is `strata-oracle:25be4a3` (branch `shard-vectors`), local only, not merged and with no GitHub remote.
- Risk normal: result identity across shards (the auditor's mapping).

## May land early

None.
