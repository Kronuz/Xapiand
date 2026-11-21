# script-method: journal

State: waiting on jobs 261120-213044-76b55a 261120-213045-ab7b83
Quest: .campaign/xapiand/quests/script-method.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Test job 80c029 passed (test_lua, 79 s). Waiting on the mutant job (76b55a, must fail) and the suite (ab7b83) at
Xapiand:dafc92de3; CI runs on the pushed branch.

## Next

1. Check the mutant's log fails on "`_method` was , PUT expected" (not a crash), then paste the
   brief's table and set "ready for review" once CI (`Build + unit + smoke`, `Functional E2E
   (docs)`) is green.

## Blocked on

Nothing.

## Paths that index and the method each gets

`DatabaseHandler::method` (handler.h) is what `call_script` hands the script.

| Path | Entry | `_method` |
|---|---|---|
| PUT /idx/id, POST /idx/, RESTORE /idx/id (one document) | write_document_view -> index / index_uploaded | "PUT", "POST", "RESTORE" |
| PATCH, UPDATE, UPSERT /idx/id | update_document_view -> patch / update | "PATCH", "UPDATE", "UPSERT" |
| PUT/PATCH/UPDATE/UPSERT /idx/ (settings, schema) | write_database_view | the request's (indexes no document) |
| RESTORE /idx/ (bulk lines) | restore_database_view -> DocIndexer -> prepare_document | "RESTORE"; a line's `_op_type` other than index: uppercase ("PATCH", "UPDATE", "MERGE") |
| CLI `--restore` (no request) | main.cc -> DatabaseHandler::restore_documents -> DocIndexer | as the RESTORE request: "RESTORE" or the line's `_op_type` |
| Plain restore lines prepared in chores | DocIndexer::index -> prepare_plain (placement handler) | runs no script (a script's lines take prepare_inline) |
| Index settings in .xapiand/indexes | index_resolver_lru.cc -> update | "" (node's own write; docs say so) |
| Foreign schema documents the node stores | schemas_lru.cc -> update | "" (node's own write; docs say so) |

No replication or internal reindex path exists in src/ today (grep finds none).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261120-213043-80c029 (test_lua), 261120-213044-76b55a (--fails-without at mutant Xapiand:044f33f8e) |
| A2 | same jobs: the CLI restore case (no request) in test_a_script_reads_the_method_of_its_write |
| A3 | suite 261120-213045-ab7b83; ci:Build + unit + smoke, ci:Functional E2E (docs) pending |
| A4 | `docs/src/content/docs/reference-guide/documents/scripting.md` (Variables table, the paragraph after it, and an e2e-checked example) |

## Result

`_method` is the write's method. Xapiand:dafc92de3 sets `DatabaseHandler::method` in the three
write views (`method_name`, the uppercase spelling from `METHODS_OPTIONS`), to "RESTORE" in
`DocIndexer`, and per line in `prepare_document` from `_op_type`. Test:
`tests/test_lua.cc` `test_a_script_reads_the_method_of_its_write` (a node in process: PUT, POST,
PATCH, UPDATE, RESTORE over HTTP, then `restore_documents` from a dump file).

## Rulings I made

1. A restore line's method is its `_op_type` uppercased when it names one other than `index`
   ("PATCH", "UPDATE", "MERGE"); `index` or none keeps the request's ("RESTORE"). The values
   are the op names, not a mapping to HTTP methods (bulk `update` upserts; mapping it to UPSERT
   would surprise).
2. The CLI restore gets "RESTORE", the method of the write that causes it.
3. The node's own writes (index_resolver_lru.cc, schemas_lru.cc) pass "": both files are outside
   the file set and no user script runs on .xapiand records; the docs state it, per the quest's
   decision.
4. The method is a public member set by the caller, not a constructor argument, so the DocIndexer
   constructor and test_loop_yield.cc stay as they are.
