> Status: CURRENT (2026-11-03) requests as tasks on the reactors as built on `phase7c/http-wave` (http H1 and H2, reactor R); the design is `docs/design/phase7c.md` D2, D3, D9, D12 and http `docs/design/task-handlers.md`. Not reviewed.

# Requests on the reactor

## What

A request is a task on the reactor that accepted it. `SearchService::handle` is a `task::task` that the connection awaits as its
child on the connection's loop; it awaits the route's view as a `task::task` on that loop's scheduler, and while the view waits (for a turn, a chore, the next
chunk of the body, a slice) the loop serves other connections. Nothing on the request path waits on a thread: the only work that
leaves a loop is a chore (a device read or write, a commit, a parse) on the host's `WorkPool`.

## How

```
 connection coroutine (reactor r)
   headers complete ── AdmissionGate (1,000 in flight, 503 + Retry-After) ── create_extension (256 demoted, 503 + Retry-After)
   handle(): hreq.cancel_token() -> budget ── co_await counted(host r, view task)
                                                    │  resumes through r's scheduler (the reactor's task::asio_scheduler,
                                                    │  wrapped by its AsioHost: fresh work first, demoted handles one per pass)
        view ── co_await turn / open / chore ──────┘  a chore completes on a worker and posts the resume back to r
        search ── co_await prospect::search_task(..., budget)  (200 us slice; yields at window boundaries)
```

1. **A host per loop.** Each HTTP reactor has an `engine::AsioHost` (`manager.cc`), bound to the loop's thread before the loop
   serves anything; the control loop has one too. A host wraps the reactor's own `task::asio_scheduler` (`Reactor::scheduler()`,
   the one `Request::scheduler()` names) and adds a timer, and `Host::current()` finds it from any task running on that thread.
   `engine::counted` returns the view's task to be awaited in place in Release builds. A Debug build runs it on an
   `engine::Counted` scheduler of the request's own, which forwards to the loop's and sets the request's `TaskCounters` around
   every resume (Asio completions included: it never resumes inline), which is how `Viewed`, `take_turn`, `ensure_open` and
   `drain_readers` enforce the wait rule (`docs/mechanisms/reader-views.md`); it moves back to the loop before it returns. http's
   affinity rule (the writer and the body stream check the loop's thread, not the current scheduler) lets the views write and read
   the body from the `Counted` scheduler.
2. **Two admissions.** http's `AdmissionGate` counts requests from headers complete to response settled (1,000 per reactor, the
   old offload queue limit). `SearchService::create_extension` refuses a request when its loop has 256 demoted handles (an
   estimate): both answer 503 with `Retry-After: 1` before any body byte is read.
3. **Cancel reads, not writes.** http requests the request's token (`Request::cancel_token()`) on a client's EOF and at
   shutdown, and `handle` gives it to the request's budget. A body read passes it too: `Request::read_chunk` awaits
   `BodyStream::read(chunk, token)`, which ends with `operation_aborted` and `read_chunk` raises `task::cancelled`; a body that
   failed (the client left mid-body) is a `ClientError`. The budget's token also reaches the index's turn waits
   (`lock_shard`, `Shard::cancel`, `take_turn`), so a read's refresh turn ends at once; a write's budget has no token and its turn wait
   ends only at `DB_TIMEOUT` (503). A search stops at its next checkpoint (`task::cancelled`, answered 503 to a client still there). A write that was admitted
   completes. RESTORE passes the budget to its indexer, so a cancel ends it at the next object and the closing commit keeps what was
   indexed. `handle` counts each request a cancel stopped (`xapiand_http_requests_cancelled` in `/:metrics`, with the
   admission and demotion peaks: `docs/mechanisms/cancellation.md`).
   At a node's shutdown `HttpAsioService::stop()` is phase 1 of task's four teardown phases: it requests every token, waits one
   grace, closes the connections of what is left, and waits one more grace. `join()` sizes the grace from what D16's budget has
   left (`XapiandManager::http_shutdown_grace`, `docs/mechanisms/control-plane.md`: 15 s of a 35 s budget), so an admitted write waiting for its
   turn finishes while the budget has time. A request still suspended after that is
   destroyed by `destroy_roots()` (phase 3) once `IndexChores::cancel_all` has drained the chores (phase 2 is `close()`), before the indexes close (`XapiandManager::join`).
4. **Sliced work.** `Request::budget` is a 200 us slice that restarts on every resume. A search takes it through
   `DatabaseHandler::budget` and `SearchRequest::budget`; the bulk indexer takes a checkpoint after each object.

## Bodies

- **Buffered** (up to 16 MiB, `BUFFERED_BODY_CAP`, http answers 413 past it): a body past 64 KiB is decoded in a chore of the
  request's own `IndexChores` group before the view runs (`Request::decode_large_body`), so a large document costs its loop the hand-off.
- **RESTORE** (NDJSON or MsgPack, no id) streams: http feeds a `BodyStream`, and `Request::next_batch` reads a chunk with
  `BodyStream::read`, parses it in a chore of the restore's group, and returns the objects. `restore_database_view` runs
  `DocIndexer::index(batch)` for one batch while `next_batch` reads and parses the next (`task::when_all`): the parse overlaps the
  indexing. `DocIndexer` prepares and applies each object as tasks, one turn per object, and records its result; a bad line is
  skipped and logged. The closing commit runs on every exit. The CLI `--restore` uses the same indexer through `engine::sync_wait`.
- **A stored blob past the cap** (a `PUT` or `POST` with `Content-Length` past it and a content type that is not JSON, YAML or
  MsgPack) streams to the data storage (D9): `DatabaseHandler::upload_stored` finds the shard the document's id goes to, takes
  a fresh volume number from the entry's `next_volume_` (the shared writer's rollover takes from the same counter, so no volume
  has two writers), and writes the record through storage's `StreamWriter` in flush chores of the upload's own group, one per
  chunk. The record is uncompressed and framed like any `stored` record (content type, then the blob). The last chore commits:
  the data and the volume header are synced. Then `index_uploaded` indexes the document with its locator, in a turn of its own,
  so no turn is held while the body arrives. A failure or a client that leaves removes the volume in a chore. A size past
  4 GiB (framing included) is a 413; a streamed blob needs an id in the URL (a numeric id that is new would be placed by the shards'
  load, after the upload) and answers 400 without one; any other view that finds a streamed body answers 413.

## FLUSH

`DatabasePool::flush` marks every unused entry `closing`, runs one close task per entry and awaits them with `when_all`; the
view holds no turn. `?clients` shuts the node down as before.

## Limits

- A streamed upload creates a volume per blob; a crash between the upload and the document's apply leaves a volume nothing names
  (storage's `reclaim_volumes` collects it; nothing calls it yet). A document whose apply fails (a version conflict) leaves its blob the same way.
- A blob reads back whole into memory (`storage_get_stored`), about three times its size at the peak (the record, the blob cut from it,
  the response), so a 4 GiB blob costs about 12 GiB to read; streaming a large blob out in chunks (D9, reads) is not built.
- The shard an upload goes to is chosen from the id, and the document's shard is chosen again when it is indexed; `index_uploaded`
  fails (`internal`, after the write) if they differ, which no case tried has produced.
- `prepare_document` runs on the reactor, sliced by one checkpoint per object: a single object that takes long to prepare is not
  interrupted. A restore into one index uses one reactor's CPU for preparing and indexing; the parse is the part that leaves it.
- A Lua script's foreign read still waits on its reactor (`engine::BlockingAllowed`, X4).

## Code

`src/server/search_service.{h,cc}`, `src/server/search_views.{h,cc}` (`handle`, `create_extension`, `wants_body_stream`,
`Request::next_batch`, `parse_ahead`, `feed_whole_body`, `read_chunk`, `decode_large_body`, `restore_database_view`,
`write_document_view`, `index_streamed_blob`), `src/engine/host_asio.h` (`AsioHost`, `Counted`, `counted`), `src/database/handler.{h,cc}` (`DocIndexer`,
`upload_stored`, `index_uploaded`), `src/database/shard.cc` (`StoredUpload`, `ShardEndpoint::upload_begin`, `next_volume_`),
`src/database/pool.cc` (`flush`, `IndexChores`), `src/manager.cc`.
Tests: `tests/test_reactor_requests.cc`, `tests/test_pool.cc` (`test_flush_closes_every_idle_index_before_it_returns`),
`tests/test_blocking_guard.cc`; `harness/stored_stream.sh`, `harness/restore_stream_check.sh`.
