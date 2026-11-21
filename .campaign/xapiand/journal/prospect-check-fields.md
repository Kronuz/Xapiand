# prospect-check-fields: journal

State: waiting on jobs 261121-114531-b60a00 261121-114518-396743
Quest: .campaign/xapiand/quests/prospect-check-fields.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code done and pushed (prospect:abfeeda, branch quest/xapiand/prospect-check-fields). Waiting on A1 and build-test jobs.

## Next

1. When the jobs pass: fill Evidence from `campaign brief`, set ready for review; A3 needs CI on the pushed branch.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | job 261121-114531-b60a00: fielddict_media_test (check_fields through a segment's reader: ids past the table, key field own or pooled throw; a big enough table passes) and fielddict_test |
| A2 | build-test job 261121-114518-396743 |
| A3 | CI on abfeeda |

## Result

inverted_plugin::reader::check_fields forwards to fielddict::reader::check_fields (include/prospect/reader.hh). Test in test/fielddict_media_test.cc; docs/mechanisms/fields.md row updated. Strata's map-time caller is quest field-check-at-map.

## Rulings I made

- Brought over only the forwarding. The prototype's fielddict.hh fast path (two seeks for tables up to 240 fields) is an optimization of the dictionary pass, not named by the quest, and stays on the explore branch.
- The test lives in fielddict_media_test (its segment already has own, pooled and multi-byte-id fields), not a new binary.
