# http-limits review: correctness

Commit: http:121aa417d32566ac80b9deff7bd7d99af943f040
Verdict: CHANGES
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261119-160805-c2cd28: TSan suite at http:121aa41, 14 of 14, `limits_test` among them (every
  section plain and TLS).
- The eight mutant jobs the journal lists (261119-155941-ca6165 through 261119-155953-7e73c2),
  each failing `limits_test` at http:96f9fe0's code: A1 (max_clients, max_body, tls_handshakes
  never applied), A2 (close_idle never walks; busy connection ignores it), A3 (flag never set;
  the gate back at headers-complete), A4 (header deadline kept over the handler). They show the
  tests fail without what they test; I did not rerun them.
- 261119-160804-917397: plain suite at http:121aa41, passed.

## What I checked myself

Read against `git -C http diff origin/main...HEAD` and reactor 6fdd45f (`build/_deps/reactor-src`),
task's `deadline` (`task.hh:1306-1325`):

1. **F1 gate.** `first_byte()` (`http_asio.h:943-948`) runs on every path a request starts:
   pipelined bytes at the loop top (`:1049-1052`) and the first read of an idle wait
   (`:1070-1073`), with no suspension between the read's resumption and the gate. The fence is
   placed right: the gate's relaxed `fetch_add`, then a seq_cst fence, then a seq_cst load of
   `closing`. If that load reads false it precedes `stop_accepting()`'s seq_cst store in S, the
   fence precedes the load, and the store is sequenced before `wait_requests()`' seq_cst read of
   `inflight` (`reactor.h:893-905`), so by the fence rule that read sees the increment or a later
   value. If the load reads true, the lease is dropped and the request is refused 503 at
   headers-complete. Bytes that arrived before the stop but whose `first_byte()` ran after it are
   refused, not lost: the invariant is decided at `first_byte()`, not at the byte's arrival.
2. **The gate is always left.** `admitted` is an RAII lease in the session frame: reset after each
   settled reply (`:1178`, `:1225`), replaced by move-assignment at the next `first_byte()`, and
   destroyed on `co_return` (read error, EOF, timeout, peer reset), on a `RequestRejected` or any
   other exception (it lives in the `try` block), and when `destroy_roots()` destroys a suspended
   frame. A TLS handshake failure never reaches `serve_connection_body`, so it never enters;
   the handshake slot is its own RAII (`http_asio_tls.h:313-317`). An idle wait holds no lease:
   the previous reply reset it, and the `IdleWait` is reset before `first_byte()`.
3. **Timeouts.** One `std::optional<task::deadline>` per frame, emplaced per read and reset by
   `settle_read()`; `~deadline()` disarms and drops its wheel entry, and the fire callback
   requests nothing once disarmed, so no timer outlives or fires after its read or session. The
   handler and the reply's writes carry no read deadline, so a held poll trips neither header nor
   inactivity (TLS too: its watch arms a read with no deadline).
4. **`close_idle()`.** The epoch bump happens before the post, the walk runs on the loop that owns
   the list, and the check-then-add at the idle entry (`:1061-1062`) runs on that loop with no
   suspension, so either the walk runs first and the check sees the new count, or the walk finds
   the wait and cancels it. A walk that lands after the read completed with data (`cancelled_`
   set too late) lets the request run; `wait_for_read()` clears the flag for the next read and the
   reply says `Connection: close`.
5. **`stop_accepting()` and keep-alive.** After the flag, a reply frames `Connection: close` and
   breaks, an idle connection's next request is refused 503 with no `Retry-After`, and a session
   started after the flag reads its request and refuses it (http:121aa41).
6. **Refusals and defaults.** `checked()` refuses any timeout outside zero to 24 h (all six) and a
   `max_in_flight` past `int` before `server_` is built (member order); `checked_tls()` refuses a
   context with a zero handshake deadline or cap. Defaults: `max_clients` 1024 and
   `max_in_flight` 1024 (note's Settled 5, D72), idle 75 s (D72), header 30 s and inactivity 60 s
   (Settled 1), `shutdown_grace` 5 s (D77), `max_body` 16 MiB, TLS 5 s and 64, as the note's
   struct.

Probe (`.scratch/xapiand/review-http-limits-correctness/`: `stop_race.cc`, `run.sh`, `run2.sh`):
eight keep-alive clients on two loops fire `/` and a 1 ms `/slow`, some split mid-headers, while
the main thread runs `stop_accepting()` then `wait_requests()`. Per round it checks that no handler
is running when `wait_requests()` returns true, that none starts later, and that every request on
a connection already served gets a reply (200 or 503).

- 261119-172252-e6337c (`run.sh`): 40 plain rounds and 10 TSan rounds. The gate invariant held in
  every round (no handler running at the drain, none started after it) and TSan was silent. But
  13 plain and 2 TSan requests on served keep-alive connections were closed with no reply (F1).
- 261119-172632-3a5769 (`run2.sh`): 40 rounds at HEAD with drop diagnostics: 8 drops, each 66 to
  404 µs after `stop_accepting()`, each after a `200` that did not say `Connection: close`. The
  same 40 rounds with the two post-write `|| !stays_open()` removed: 0 drops, 103 refusals.
- 261119-162948-db6eff was my first try and hit its deadline (my probe's timer slept a fixed 5 s a
  round); it proves nothing.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | The keep-alive decision is read twice. A reply is framed with `keep_alive && stays_open()` (`http_asio.h:1222`; streamed `:1169`), and once it is written the connection breaks on a second `!stays_open()` (`:1226`, `:1179`). A `stop_accepting()` between the two closes a connection whose reply said keep-alive. The client's next request then meets a closed socket instead of the 503 + `Connection: close` that A3 and the journal's ruling promise ("an explicit refusal instead of a reset it cannot retry safely"). This quest introduced it. | 261119-172252-e6337c, 261119-172632-3a5769: 8 to 13 drops in 40 rounds at HEAD, 0 with the re-check removed | Break on the framed decision only: drop `|| !stays_open()` from both breaks. `closing` then refuses the next request with 503 at `first_byte()`, and `close_idle()` still closes at the idle entry (`:1061`), as the docs already say ("or closes at its next idle wait"). Add `stop_race` (or the same loop) to `limits_test`: fail on any reply-less close of a served connection, and report the 503 count reached. A mutant restoring the re-check must fail it, since it failed in 8 and 13 of 40 rounds here. |
| F2 | no | gap | The gate now holds a slot from a request's first byte, so a client trickling headers occupies `max_in_flight` for up to `header_timeout` (30 s). That was not true at headers-complete. With the defaults (`max_clients` 1024, at most `max_in_flight` 1024 per loop) a loop cannot fill. A service with `max_clients` 0, or with more clients than one loop's cap (Xapiand: 1024 clients, 1000 per loop), can have its gate filled by slow-header peers and answer 503 to the rest. The journal names the cost. | `http_asio.h:943-948`; journal "Rulings" F1 | Owner's call: keep (bounded by `header_timeout` and `max_clients`), or state the bound in TRANSPORT.md beside `max_in_flight`. |
| F3 | no | nit | `request_timeout`'s comment says "from a request's start", but its clock starts at the loop top, before the idle wait, so the idle time comes out of the request's budget, and an idle connection closes at `request_timeout` when that is under `idle_timeout`. This behavior predates the quest; only the new comment states otherwise. | `http_asio.h:115-116`, `:1043-1045` | Say "from the wait for its first byte", or start the clock at `first_byte()`. |
| F4 | no | nit | `stays_open()` reads `closing` relaxed, so a reply framed just after `stop_accepting()` can still say keep-alive. Once F1 is fixed that is harmless: the next request reads the flag seq_cst at `first_byte()` and is refused 503. | `http_asio.h:954` | None after F1; keep it relaxed. |
| F5 | no | nit | A connection accepted before `close_idle()` whose session starts after it (still queued, or in its TLS handshake) reads the new epoch and stays open. This fits "connections waiting for a request", but the docs do not say it. | `http_asio.h:951`; `docs/mechanisms/coroutine-handlers.md` "Closing idle connections" | One clause in the mechanism note. |

## Not run

- No mutant reruns; no CI (not pushed).
- The probe runs on plain HTTP only. TLS shares the code path at fault (`serve_connection_body`), and
  `limits_test` covers TLS deterministically.
- No stress of `close_idle()` against arriving requests: closing an idle connection while a request
  is in transit is the documented behavior ("at once"), so a reply-less close there is not a defect.
- `DeferredReply` and `user_data` paths, per the lead (Detent's http-drop-user-data merges later).
- No measurement of the per-request fence or the per-read deadlines the defaults now add.
