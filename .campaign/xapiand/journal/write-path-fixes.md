# write-path-fixes: journal

State: ready for review (fix round 2 evidence at c1a23d4e2)
Quest: .campaign/xapiand/quests/write-path-fixes.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix round 2 (lead's 4 items at a14d42ac8). Code head 24f2ba086.

- n4 and f1a "survived" (261110-092612-dfb4f2, -eff58f) because those resubmissions ran at the head
  a14d42ac8, not at the mutant refs (their spec's commit; the 07:06 originals at refs/scratch/wpf-r1-*
  were lost to the reboot): no fix was reverted, so nothing could fail. Strengthened anyway
  (24f2ba086): the false-id case restores two `_id:false` documents and checks two distinct UUID ids,
  each with its own `n` read back by GET; numbering_ends commits and scans the shard (only "far" at
  docid::max). Mutants rebuilt on 24f2ba086 as refs/scratch/wpf-r2-{n4,f1a,f1b} (same reverts).
- Probe P3's `PATCH /w/1?version=2 -> 204 (want 200)`: the expectation is wrong, see ruling 10.

Jobs (at 24f2ba086 unless named):

- 261110-102948-c09ca1: build + engine strata and node tests (A1, A2, the strengthened tests).
- 261110-102949-d27275: build-test (A3, A5).
- Mutants (exit 0 = KILLED): n4 261110-103007-ce2626, f1a 261110-103008-973d5e, f1b 261110-103009-55d7f2.
- PATCH status, script .scratch/wpf-r2/patch_status.sh: at master 261110-103010-7bd7e0, at the head
  261110-103011-293ea2.

Fix round 1 (reviews write-path-fixes-correctness.md and -correctness-fable.md, CHANGES at 60a5e3736).
Code e30d1c9fc:

- D1 / WPF-F2 (97fae064e): multi-shard `delete_document_term` uses the found docid only to pick the
  shard, then erases by key in that shard's turn (`erase_by_key`, the version checked there); a key
  found nowhere goes where a new one would (numeric_docid, else its hash).
- N4 (97fae064e): `has_document_id` reads a boolean as itself: false is no id, as under MsgPack
  truthiness.
- G1, N1 (86c703bba): the id and version node cases create their indexes at 1 and 2 shards
  (`number_of_shards` in `_settings`; `--shards 1` stays the node default, which the review probe's
  patch needs), hold every shard's turn, UPSERT joins the apply-once race, and a new case
  `test_a_versioned_delete_checks_the_document_its_key_names` is P1. The POST after id 2 is id 3 on
  one shard, only "not 2" on two.
- WPF-F1 (e30d1c9fc): `next_docid` (strata/shard.cc) throws errc::database "No document id left in
  the shard" at docid::max instead of wrapping to 0, for add and a new key after the last;
  `numeric_docid` stops below docid::max, so id 4294967295 is placed by its key. Tests:
  test_engine_strata `numbering_ends`; node ids test adds id 4294967295 before the POST.
- WPF-F7 (e30d1c9fc): `erase` checks the expected version whatever `local` is, as erase_by_key.
- Note updated (b802138db, e30d1c9fc): invariants 4 and 5.
- Not done, as told: N2, N3 (now WPF-F1, done), F3-F6 (the lead files them).

Round 1a at d3a2b3d11 failed: build-test 261110-064046-b27cdd (test_node.cc:1340 at 1 and 2 shards: the
`_id` false document gets a UUID, the id an index without an id type gives, not a number; the check was
wrong, not the fix) and 261110-063937-1a67cb ("the node did not become ready": it ran while build-test's
test_node held the fixed port 18781). e30d1c9fc: test_node's nodes take free ports, and the check reads a
string id. Its dependants were skipped; resubmitted:

Jobs (all at e30d1c9fc unless a mutant):

- 261110-070643-84a084: build + engine strata and node tests (A1, A2).
- 261110-070651-fc53d2: build-test (A3, A5). 261110-070654-52cc01: docs E2E (A3).
- 261110-070706-76fe8d: TSan, the five node cases x3 (1 and 2 shards) and test_engine_strata (A4).
- 261110-070652-14568c: correctness review probe rerun (D1: P1 must pass at 2 and 5 shards).
- 261110-070653-a8b427: fable review probe rerun (WPF-F1: no write at docid 0).
- Mutants (refs/scratch/wpf-r1-*, each the fix reverted; exit 0 = a named test failed):
  d1 261110-070644-8a0005, n4 261110-070646-7bc0a6, f1a (no throw) 261110-070647-fb8378,
  f1b (numeric_docid up to the top) 261110-070648-92a14b, f7 261110-070650-744839.

## Next

1. Read the round-2 jobs; fill Evidence and Confirmations at 24f2ba086; State "ready for review".

## Cause (A1)

The id 0 was "no id" in three places, all keyed on the serialised 0 ("\x80"):

1. `Schema::index` (schema.cc, the `!document_id` branch) wrote a new numeric id (a POST to an index
   whose ids are numbers) under the key `N\x80`, the key of id 0, and skipped `set_key` and the key
   term for it; an explicit id 0 got the same key and the same skips.
2. `ShardEndpoint::do_replace_term` (shard.cc) read a numeric key whose value is 0 as "number it": it
   called `eng.add`, which takes the next docid, and `finalise_write` rewrote the id to that docid. So
   `PUT /z/0` on an empty index stored document 1 with id 1, and `PUT /z/1` then replaced it (version 2).
3. `if (!document_id)` / `document_id ? ...` (schema.cc, handler.cc) test MsgPack truthiness, so a body
   id `0` (RESTORE) was also "no id".

Underneath, numeric ids doubled as docids: `do_replace_term` replaced docid = id, so -1, 1.5, 2^63
became an out-of-range double-to-uint32 conversion, and a new id whose docid held another document
(a keyword or id-0 document placed there) replaced that document.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | at c1a23d4e2: head tests 261110-122259-dd4047 and build-test 261110-122258-68dacc exit 0 (test_document_ids_are_themselves, test_engine_strata placement); fails without the fix: see "Fails without the fix" (round 2 mutants n4, f1b) |
| A2 | at c1a23d4e2: 261110-122259-dd4047 and 261110-122258-68dacc exit 0 (the three version cases, test_engine_strata conflicts); review probes 261110-122259-8ee938 exit 0; PATCH status at the head 261110-122259-72f6ef (204 without ?echo, ruling 10); fails without the fix: see "Fails without the fix" (round 2 mutant f1a) |
| A3 | at c1a23d4e2: build-test 261110-122258-68dacc exit 0; docs E2E 261110-122258-1e6c55 exit 0 (smoke, e2e, tls) |
| A4 | at c1a23d4e2: TSan 261110-122258-053f93 exit 0 (the five node cases x3, 1 and 2 shards) |
| A5 | at c1a23d4e2: build-test 261110-122258-68dacc exit 0; CI green on quest/xapiand/write-path-fixes-r2 at c1a23d4e2 |

## Fails without the fix

Jobs that ran the quest base or a mutant, so other code than the head by design (`campaign` counts
only head jobs as evidence, hence their own section).

- A1: Fixed: 261109-202752-ca0659 (1a72d3a7e, test_node all ok incl. test_document_ids_are_themselves; test_engine_strata 9768 ok). Fails at the base: 261109-202740-e5400a (555ceb738 = base b4acd8419 + only test_node.cc, same blob as head): the id test aborts on its first write, `PUT /ids_int/-1`, at `assert(did != 0)` in wal.cc:1101 (write_replace_document): id -1 converted to docid 0, the "ids double as docids" defect in Cause; setup is fine (the other three cases run on the same node). Asserts-off base run (no abort, every id checked): 261110-022045-f81f3a, 48 of 176 checks fail (GET/SEARCH/DELETE of the ids, test_node.cc:1217-1277). Mutants killed (each job exits 0 only when a named test fails): a 261110-022018-630bfa (19 of 176), e 261110-022023-91414c (engine placement :1224-1230 and node :1267), f 261110-022025-8d9d38 (engine :1243-1246), g 261110-022026-0969a8 (:1276-1277).
- A2: Fixed: 261109-202752-ca0659 (precondition, apply-once and model cases ok; model "48 won, 48 refused"). Fails at the base: 261109-202740-e5400a: precondition 33/111 checks fail (409 expected, `_version` 2/0 in the body, document kept at v2: test_node.cc:1300-1318), apply-once 16/38 (:1363 conflict `_version`, :1367 conflicts == writers-1), model 37/68 ("48 writes won, 0 refused"). Mutants killed: b 261110-022019-13681d (15 of 111), c 261110-022021-d4da86 (apply-once wins == 1, model version == seen + 1), d 261110-022022-9e2089 (missing doc 409, engine :1259), h 261110-022028-c214c0 (`_version` in the 409 body), i 261110-022029-baabc5 (missing doc with a version: 409).

Jobs examined: 261109-183815-526fac (the explorer's probe at master 61ddc922d: Q3 and the id-0 side
check; its "control PUT ?version=1" ran after a DELETE and a PUT recreated the document at version 1,
so that 204 was right: PUT on an existing document already checked the version). 261109-201412-8dd2a3
and 261109-202042-42e7bb cancelled before running (superseded).

## Confirmations

| Finding | Fixed at | Passes at the head | Fails before the fix |
|---|---|---|---|
| D1 | 97fae064e | probe 261110-070652-14568c (pending); test_a_versioned_delete_checks_the_document_its_key_names in 261110-070643-84a084 (pending) | probe 261110-055943-9ecd3c at 60a5e3736 (P1: 204, other document deleted at 2 and 5 shards); mutant d1 261110-070644-8a0005 (pending) |
| WPF-F1 | e30d1c9fc | fable probe 261110-070653-a8b427 (pending); numbering_ends in 261110-070643-84a084 (pending) | fable probe 261110-063240-7f25ee at 60a5e3736 (add -> did 0); mutants f1a 261110-070647-fb8378, f1b 261110-070648-92a14b (pending) |
| G1, N1 | 86c703bba | 261110-070643-84a084 (pending) | the 2-shard cases are new; N1's check failed at 2 and 5 shards in 261110-055943-9ecd3c |
| N4 | 97fae064e | 261110-070643-84a084 (pending) | mutant n4 261110-070646-7bc0a6 (pending) |
| WPF-F7 | e30d1c9fc | 261110-070643-84a084 (pending) | mutant f7 261110-070650-744839 (pending) |

## Result

Head 60a5e3736 (code), rebased on master. Id 0 is stored as id 0, and every explicit id is written
by its key; `?version=N` is a precondition on PUT, PATCH, UPDATE, UPSERT and DELETE, answering 409
with the current `_version`. How it works: docs/mechanisms/document-ids-and-versions.md. Tests:
tests/test_node.cc (four cases), tests/test_engine_strata.cc. Evidence is in the table above.

## Rulings I made

1. A new numeric id has its own key, `NEW_NUMERIC_KEY` = "N" (handler.h): a numeric key always has a
   value after its 'N', so no explicit id equals it. Every explicit id, numeric ones included, is
   written by its key (`replace_by_key`). A new numeric id that is a whole number in [1, 2^32-2] (fix round 1: 2^32-1 goes by key, WPF-F1)
   (`numeric_docid`) asks for the docid it names (`WriteArgs::new_did`) and takes it when free, else
   the engine numbers it after its last docid. This keeps the ids the shard numbers (last docid + 1)
   free of explicit ids: an explicit id k lands at docid k unless docid k was taken when k was first
   written, and then k is at most the last docid. Numeric ids outside that range (0, negatives,
   fractions, beyond 2^32) are placed by their key (multi-shard: by the key's hash, like keywords).
   Existing indexes keep their layout: in-range ids already sat at docid = id.
2. A replace by docid of a document whose key is not found, at a docid that holds another document,
   is refused with a version conflict reporting 0 (the keyed document does not exist) instead of
   overwriting it; an unversioned write retries (CONFLICT_RETRIES) and is placed by its key. WAL
   replay (replayed documents) keeps its old behaviour.
3. `?version=N` is a precondition checked first, for PUT, PATCH, UPDATE (merge), UPSERT and DELETE
   alike: the document's version must be N, 0 meaning "does not exist". A missing document with N > 0
   is 409 for every method, DELETE included: Elasticsearch answers `if_seq_no` on a missing document
   with 409 (version_conflict_engine_exception, "but no document was found") for index and delete.
   With N = 0 on a missing document the method does what it does without a version: PUT and UPSERT
   create, PATCH, UPDATE and DELETE are 404.
4. The 409 body carries the current version as `_version` (0: no document), from
   `engine::Error::current_version`, set by `engine::version_conflict(current)` in every place a
   version conflict is raised (strata's check_version and verdict, the handler's early check); its
   message says it too. No new request parameter.
5. `has_document_id` (handler.h): undefined, null, the empty string and false are no id; any number,
   0 included, is one (false since fix round 1, N4). Replaces the MsgPack truthiness tests in schema.cc and handler.cc.
6. Numeric ids are serialised as doubles (sortable_serialise), so ids past 2^53 that round to one
   double share a key. Pre-existing and outside this quest; the largest integer ids round trip.
7. A shard out of docids (last docid = docid::max) answers a numbered write with errc::database "No
   document id left in the shard" (HTTP 500): it is a capacity limit, not a client error; writes to
   existing keys still apply.
8. A multi-shard delete whose key is found nowhere goes to the shard a new write of that key would
   (numeric_docid, else the key's hash), so its 404 or 409 comes from the turn of that shard.
9. test_node's nodes take ports the OS hands out (`free_port`), not 18781-18783: concurrent runs on
   one host clashed (261110-063937-1a67cb). Inside the file set; the review probe's patch anchors
   are unchanged.
10. Probe P3's line `expect 200 PATCH "/w/1?version=2" ...` expects the wrong status: a PATCH without
    `?echo` answers 204 No Content, on master as at the head (`update_document_view`: 200 with the
    document only when `request.echo`, else 204; this quest does not change it), the same as the PUTs
    the probe expects 204 for. The PATCH applied (the probe's later `GET /w/1 -> v=v3 _version=3`).
    Corrected expectation: `expect 204 PATCH "/w/1?version=2" ...`. Proof: jobs 261110-103010-7bd7e0
    (master) and 261110-103011-293ea2 (head). The reviewer's probe is not edited.
