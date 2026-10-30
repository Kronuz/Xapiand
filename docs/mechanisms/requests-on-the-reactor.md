> Status: CURRENT (2026-10-30) requests as coroutines on the reactors as built on `phase7c/x3-views` (phase 7c step X3); the design is `docs/design/phase7c.md` D2, D3, D9, D12. Not reviewed.

# Requests on the reactor

## What

A request is a task on the reactor that accepted it. `SearchService::handle` is a coroutine awaited on the connection's loop; it
runs the route's view as a `task::task` through that loop's scheduler, and while the view waits (for a turn, a chore, the next
chunk of the body, a slice) the loop serves other connections. Nothing on the request path waits on a thread: the only work that
leaves a loop is a chore (a device read or write, a commit, a parse) on the host's `WorkPool`.

## How

```
 connection coroutine (reactor r)
   headers complete ── AdmissionGate (1,000 in flight, 503 + Retry-After) ── create_extension (256 demoted, 503 + Retry-After)
   handle(): hreq.on_cancel(-> cancel_source) ── Run(host r).await(view task)
                                                    │  resumes through r's AsioHost  (task::asio_scheduler:
                                                    │  fresh work first, demoted handles one per pass)
        view ── co_await turn / open / chore ──────┘  a chore completes on a worker and posts the resume back to r
        search ── co_await prospect::search_task(..., budget)  (200 us slice; yields at window boundaries)
```

1. **A host per loop.** Each HTTP reactor has an `engine::AsioHost` (`manager.cc`), bound to the loop's thread before the loop
   serves anything; the control loop has one too. A host is a `task::asio_scheduler` plus a timer, and `Host::current()` finds it
   from any task running on that thread. `engine::Run` runs one request's task on it: `Run::await` is `task::as_awaitable`
   over the request's own scheduler. Release builds resume straight through the host. A debug build gives each request a
   `TaskCounters` and sets it as the running one around every resume, which is how `Viewed`, `take_turn`, `ensure_open` and
   `drain_readers` enforce the wait rule (`docs/mechanisms/reader-views.md`).
2. **Two admissions.** http's `AdmissionGate` counts requests from headers complete to response settled (1,000 per reactor, the
   old offload queue limit). `SearchService::create_extension` refuses a request when its loop has 256 demoted handles (an
   estimate): both answer 503 with `Retry-After: 1` before any body byte is read.
3. **Cancel reads, not writes.** http calls `Request::cancel` on a client's EOF or at shutdown; `handle` turns it into the
   request's `task::cancel_source`. A search stops at its next checkpoint (`task::cancelled`, answered 503 to a client still
   there). A write that was admitted completes. RESTORE passes the budget to its indexer, so a shutdown ends it at the next object
   and the closing commit keeps what was indexed.
4. **Sliced work.** `Request::budget` is a 200 us slice that restarts on every resume. A search takes it through
   `DatabaseHandler::budget` and `SearchRequest::budget`; the bulk indexer takes a checkpoint after each object.

## Bodies

- **Buffered** (up to 16 MiB, `BUFFERED_BODY_CAP`, http answers 413 past it): a body past 64 KiB is decoded in a chore of the
  request's own `IndexChores` group before the view runs (`Request::decode_large_body`), so a large document costs its loop the hand-off.
- **RESTORE** (NDJSON or MsgPack, no id) streams: http feeds a `BodyStream`, and `Request::next_batch` reads a chunk with
  `task::await_asio`, parses it in a chore of the restore's group, and returns the objects. `restore_database_view` runs
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
- A blob reads back whole into memory (`storage_get_stored`); streaming a large blob out in chunks (D9, reads) is not built.
- `prepare_document` runs on the reactor, sliced by one checkpoint per object: a single object that takes long to prepare is not
  interrupted. A restore into one index uses one reactor's CPU for preparing and indexing; the parse is the part that leaves it.
- A Lua script's foreign read still waits on its reactor (`engine::BlockingAllowed`, X4).

## Code

`src/server/search_service.{h,cc}`, `src/server/search_views.{h,cc}` (`handle`, `create_extension`, `wants_body_stream`,
`Request::next_batch`, `parse_ahead`, `feed_whole_body`, `read_chunk`, `decode_large_body`, `restore_database_view`,
`write_document_view`, `index_streamed_blob`), `src/engine/host_asio.h` (`AsioHost`, `Run`), `src/database/handler.{h,cc}` (`DocIndexer`,
`upload_stored`, `index_uploaded`), `src/database/shard.cc` (`StoredUpload`, `ShardEndpoint::upload_begin`, `next_volume_`),
`src/database/pool.cc` (`flush`, `IndexChores`), `src/manager.cc`.
Tests: `tests/test_reactor_requests.cc`, `tests/test_pool.cc` (`test_flush_closes_every_idle_index_before_it_returns`),
`tests/test_blocking_guard.cc`; `harness/stored_stream.sh`, `harness/restore_stream_check.sh`.
