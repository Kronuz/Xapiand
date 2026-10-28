> Status: CURRENT (2026-10-27) exploration of three numeric-range review findings (docs/reviews/numeric-range-code.md CR3, CR3 aside, CR5.5). Branch `explore/value-compare` (Xapiand:bdc46625d and the harness commit after it). Probes: `.scratch/value-compare/` in the main checkout.

# Value-slot comparison, two-field open range, RESTORE first document

## 1. The value slot compares some integers inexactly: the cause is `sortable_serialise`

Not the conversion, not the terms. The value source compares the bytes `sortable_serialise` returns, and that function returns wrong
bytes for some inputs. Black-box evidence (`ss.cc`, `sweep*.cc`, `neg.cc`: the function called and its output compared; the source never read):

```
2147483647 -> c5ffffffff     2147483650 -> c600000001
2147483648 -> c6             2147483651 -> c60000000180
2147483649 -> c60080         (unserialises to 2164260864; sorts after 2147483650 up to 2147483779)
```

Two defects, one cause (a run of about 30 equal mantissa bits):

1. Positive values (integer, positive, floating, datetime seconds alike: the function takes a `long double`) whose mantissa has a run of 29 or more zero bits below a set bit are encoded wrongly and do not round-trip. Smallest cases: 2^31+1 (a zero run of 30), `1 0^29 1111`, 1+2^-40 as a double, 2^31+0.5. Every 2^k+d with k >= 31 and small d is hit (all of 2^42..2^63 plus a few units), and so is 1+2^-40. Random full-entropy values are not (20000 random values per bit length L = 1..64, both signs: no failures), which is why corpora of random values and `2**k + d` for k <= 30 pass.
2. Negative values round-trip exactly but the bytes do not sort: the same run, seen in the complemented mantissa, makes the encoding of -(2^a) a strict prefix of the encoding of -(2^a)-1 for a >= 30 (`3a20` against `3a2000000180`), so -(2^a)-1 sorts above -(2^a). Also magnitudes 2^a-m (a long run of ones) for a = 30, 31 and 62. -1073741825 < -1073741824 is answered wrongly; -1073741824 itself is the pair's culprit.

Since when: always. The 2016-07-25 commit that introduced the long double algorithm (Xapiand:cdd00e367) already returns the same wrong bytes for these inputs (built from that revision and run), as do 2019 (Xapiand:d2a588afc, Xapiand:8c036cd1f), the pre-seam binaries (`.scratch/gate05/xapiand-base`, `.scratch/build/bin/xapiand` of 2026-10-13) and master.

Fix location: `src/sortable_serialise.cc` (GPL, off limits to this exploration). A fix necessarily changes the bytes of the affected values, so existing indexes hold wrong slot bytes for those values and need a reindex. A property test for the function (round trip and adjacent order over 2^k+d, -(2^k)+d, 1+2^-k, and runs of zeros and ones of every length 1..63) is the acceptance test; `sweep2.cc`, `sweep4.cc` and `neg.cc` are its first cut. No prototype: the fix is in the forbidden file.

Which values are wrong, in short: a positive value is wrong iff its binary expansion has a gap of 29 or more zero bits (so integers from 2^31+1, and doubles like 1+2^-40); a negative value only breaks order, in the same circumstance on its complement (from -2^30-1). Integers below 2^31 in magnitude are all exact.

## 2. Two custom-accuracy fields and `_to: 70`: a harness artifact, not an engine bug

`live_probe.py` names documents `<field><k>`: field `c` document 20 is `c20`, and field `c2` document 0 is also `c20` (`c2` + `0`). The `c2` documents 0..9 replace the `c` documents 20..29, so 10 `c` documents are gone, and they are not the ten most negative ones but the ones at sorted positions 20..29 (the values -455596, -418700, -365025, -337343 and so on in that corpus). With ids `<field>_<k>` (`live_probe_uid.py`) the run on the branch (Xapiand:5b4748d5c, master plus fix/numeric-range) gives 0 wrong for `c` and `c2`, with `c` alone or both. The one remaining disagreement with truth in `c` is 2^40+1 (finding 1). `td` and `td2` collide the same way (`td2` + `0` is `td20`).

## 3. RESTORE "drops the first document": it is document id 0

Not RESTORE and not the first position: a numeric document id 0 is lost, in any write. `PUT /a/0` answers 204 and `GET /a/0` answers 404; the miss.py corpus used ids `str(k)` from 0. Cause: `src/database/shard.cc:1541` treats a numeric id term with value 0 as "assign a new numeric id" (`wdb->add`, `finalise_write` embeds the assigned docid as the document's id); the user's document 0 is stored under the next free docid (1), and document 1 then replaces it (`replace((did - 1) / n_shards + 1, ...)`). The same code is in master and in the pre-seam binaries.

Prototype (Xapiand:bdc46625d): `DatabaseHandler::get_prefixed_term_id` (`src/database/handler.cc`) throws a `ClientError` for a numeric id 0, so the write, the read and the restore item answer 400 and no other document is touched. `harness/id_zero_check.py BIN` checks it (master: PUT of id 0 answers 204; fix: 400, ids 1..999 kept). Open question for the owner: reject (done here) or make 0 a normal id by giving the "assign" request another representation; rejecting is the smaller change and matches Xapian's rule that docid 0 does not exist.
