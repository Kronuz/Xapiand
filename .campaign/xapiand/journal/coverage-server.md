# coverage-server: journal

State: waiting on jobs 261120-121305-9dbdb6 (then ready for review)
Quest: .campaign/xapiand/quests/coverage-server.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Driver harness/coverage_e2e_server.py (groups per node flag set) + tests/test_coverage_server.cc written blind against an old binary; coverage.sh has a `server` workload and NOBUILD=1. Job 444b1a is the first coverage build (slot xapiand-coverage-server keeps build-cov: run the driver against its bin/xapiand to fix expectations, then NOBUILD=1 jobs). Baseline (coverage-areas job 8642ea): server 76.8/49.4, search_views.cc 76.3/48.9, mime_types.cc 88.2/66.0.

## Next

00000. 1f1407604: group `verbs` (all 30 table verbs as override header, `:verb` path command and the method itself, plus the mismatch refusals; `/:info`,`/:head`,`/:get`,`/:metrics`,`/:stats`) measured locally on the slot's build-cov (scratch ~/.scratch/xapiand/cov-server-work/cov.sh, same llvm-cov method as coverage.sh): server 90.1% lines, 85.6% branches (1423/1663; 80% needs 1331). Job 261120-121254-2ad180 is the evidence run (A1); then `campaign verify coverage-server` and ready for review. The other clusters (negotiation, flags, locators, streams) are not needed for the target and stay open. Only 3 of is_node_command's 30 verb cases are reachable (it is called only from info, head and retrieve-database views, so only :info, :head, :get reach the switch).

0000. 9bd410: server 89.4% lines (2063/2308), 61.8% branches (1028/1663). Added: mime_odd, mime_short, mime_missing (node env XAPIAND_DATA_PATH with a stray/short/absent mime.types: ~11 lines) and four more multi-index refusals in edges (~6 lines): expect ~2080 lines >= 2078 (90%). Rerun id in State. Classification below.

## Uncovered branch classification (from 9bd410's lcov: 307 uncovered branch points in search_views.cc 287, mime_types.cc 20; llvm's summary counts 635 of 1663, about twice as many because macro and template regions are counted separately, so use the shares, not the counts)

- Reachable by an E2E scenario, about 230 of 307 (75%):
  - per-verb macro cases (METHODS_OPTIONS in the method-override switch 27, the :verb command map 22, is_node_command 30, 3): a request per verb, each as override header, as `/t/:verb` and as `/:verb`; about 85.
  - serialization and negotiation (ct_types per type 16, html/x-msgpack serializer, resolve_ct_type): about 25, mostly via Accept on each answer kind; the html path itself is dead (see below).
  - request flags and parse errors (2229-2279: --pretty/--human/--echo defaults combined with the query, invalid path and query init): about 12.
  - document paths (blob/stored GET locators 1895-1951, search hits without _id/_version, UPSERT/UPDATE of stored bodies 1073-1078, selectors 989-1023, metadata 1156-1216): about 55; need documents whose stored data lacks _id/_version and PUT/UPSERT/UPDATE of blobs of each type.
  - streamed bodies (NDJSON bad line at the end of a chunk and mid-stream 2929-2983, STREAM mode 2911, 3006-3028 feed_whole_body, 3173-3212 admission): about 20, with raw sockets.
  - mime.types (52-110, 20 branches): done above.
- Needs fault injection, about 35 (11%): the restore's closing commit failing (1784-1788), an engine error other than not-found from get_mset (2171), cancelled requests (2353-2356, 3050, 3267), early end of a request body (3039-3050, 3075), a destructor/log exceptions (2772-2795), a scheduler of another host (3253), the demoted-admission cap (3141), view cancellation 503 (3260-3265), an exception after the response was emitted (3298).
- Dead or defensive, about 40 (13%): can_preview (239-270, no caller), QUERY_FIELD_TIME/PERIOD (2512-2528, no view passes them), the html serializer (2679-2682: Accept html is answered 200 empty or 406 before it), `mime_type(selector)` for a body with no Content-Type (a body without one is read as JSON, so 972/1073/1077 never type by extension), method_token default and `method_from_string` default (536, 692, 3113), Retry-After/Content-Encoding headers (344, 353: nothing sets them), NOT_ACCEPTABLE rewrites (2712-2722).
- Reachable without fault injection: about 75% of the uncovered branches, so 1028 + 0.75 x 635 = about 1500 of 1663 = about 90% upper bound; 80% needs about 300 more of those, mostly the per-verb macro cases and the document-path variants above.

000. 8d4a2b: server 87.0 lines / 57.1 branches (2008/2308, 949/1663). Added groups edges (multi-index refusals, every Accept type, odd URLs, split/chunked RESTORE bodies) and logged_bodies (--verbosity 5, access-log body renderers); rerun 9bd410. Then: read coverage-uncovered.txt again; remaining clusters seen: select/handlers at 1073-1131 (UPSERT/UPDATE of stored blobs, selectors), 1395-1411 (index settings), 1895-1950 (document/blob GET locators), 2347-2356, 2763-2786, 3246-3271 (413 for other views), 3296-3340. If 80% branches needs fault injection (cancelled tasks, 'request body ended early', reactor-binding THROW), list them in the journal as unreachable.

00. Job 101643 ran the driver: 38 failures, server 74.3/48.9. Fixed the driver's expectations (commit "Fix the server driver's expectations"); all groups pass locally on the slot's build-cov. Rerun 8d4a2b measures coverage; read .scratch/cov-server/coverage-uncovered.txt (search_views.cc lines) and add scenarios until >= 90/80. Probes: ~/.scratch/xapiand/cov-server-probe (delete at the end).

0. 095920 failed (driver not executable; mode 755 committed); rerun 261120-101643-e0b959. 092943 failed: tests/test_coverage_server.cc lacked <unistd.h> (fixed); rerun is 261120-095920-b03f29. Merged origin/master at 72547e782 (2026-11-20). Job 261120-092943-82afc7 = build + server driver only (WORKLOADS=server, out .scratch/cov-server); 444b1a was only the build (driver never ran). Read its log, then:
1.  fix the driver's expectations against the real binary, then read coverage-uncovered.txt and add scenarios until server >= 90 lines / 80 branches.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261120-121254-2ad180: server 2080/2308 lines (90.1%), 1423/1663 branches (85.6%) against 80%, was 62.7% at 261120-115902-3a4acd |
| A2 | 261120-121254-2ad180 (the driver run, every check passing): harness/coverage_e2e_server.py group `verbs`: every request asserts status and body or header values (verb_answer); no bug was fixed, so no fails-without row; the two odd answers are PIN checks with hunt rows |
| A3 | check suite 261120-121305-9dbdb6; CI at the merged head |

## Result

Group `verbs` in harness/coverage_e2e_server.py (1f1407604) adds 422 branches: server branches 62.7% -> 85.6% (job 261120-121254-2ad180), lines stay 90.1%. Open: negotiation, flag, locator and stream clusters (about 190 branches left) are not needed for the target; fault-injection and dead branches stay as classified above. Master needs merging into the branch (lead). No docs change: the driver's docstrings carry the how.

## Hunt-list rows (product behavior the driver pins as PIN checks; not fixed here)

- QUIT and FLUSH (no flags) answer 200 but never stop the node: try_shutdown(true)/shutdown(0,0) reach shutdown_sig(0) with _shutdown_asap 0, so shutdown_impl does nothing (src/server/search_views.cc quit_view/flush_view, src/manager.cc shutdown_sig).
- A blob PUT without ?commit then GET answers 500 "Storage EOF".
- Accept "application/json;indent=N" is parsed (search_views.cc ~460) but the answer stays compact.
- POST with an _id in the body ignores it and generates one (Location names the generated id).
- version=abc is read as a stale version (409), not 400; limit=abc, offset=abc, check_at_least=abc, collapse_max=abc are not refused (read as 0).
- Two q parameters are ANDed; a metadata path selector (/t/:key.a) answers 404.
- SEARCH with an Accept that names no renderable type (text/html, ndjson, image/png) answers 200 with an empty body (GET of a document answers 406).
- INFO ?raw puts raw bytes into the JSON text (invalid UTF-8).
- RESTORE of an empty array answers took 4295797.02 (an unset start time).
- INFO of an index with an empty shard (one document in five shards) answers last_id 4294967295, doc_del 4294967294, doc_len_lower near 2^32 (unsigned wrap-around of an empty shard's values); INFO of a document puts raw value bytes (\xbe) into the JSON text, not only with ?raw.
- The method override header refusal (PUT with X-HTTP-Method-Override) answers 400 with an empty body; the :verb mapping refusal answers a JSON body naming the cause.
- --max-search-indexes counts shards: /p1,p2/ with 5 shards each "names 10".

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>

Lead ruling (msg): harness/coverage.sh is coverage-core-main's; my edit is reverted (bc5fd77fa, plain commit). It will discover harness/coverage_e2e_<name>.* as workload <name> and add NOBUILD=1. Until then run the driver directly on the slot's build-cov/bin/xapiand. coverage-server lands after coverage-core-main. Job 444b1a ran at b0957c548 (with my old edit) and is only the build.
