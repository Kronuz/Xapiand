# phase7c-http-wave: Migrate HTTP handlers to task coroutines

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:0f01d54dc (2026-11-03); its ledger `docs/status/phase7c-http-wave.md` is the record. Its machine state is in `state.json`: `campaign status phase7c-http-wave`.

## Outcome

HTTP handlers and body reads use task coroutines and request cancellation tokens, while each host uses the reactor's scheduler. Shutdown stops the service, cancels and drains chores, then closes and destroys roots; the grace adapts to remaining shutdown time.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] 1 `AsioHost` wraps reactor schedulers; mutant H1own dies: `docs/status/phase7c-http-wave.md`
- A2 [artifact] 6 Link `take_turn` deadline to request token through `Shard::cancel`; mutants T6link and T6shard die: `docs/status/phase7c-http-wave.md`
- A3 [artifact] 8 Delete `Run` and `Run::await`; counters move to `engine::Counted`; mutants Ccount, Cback, Cinline die: `docs/status/phase7c-http-wave.md`
- A4 [artifact] 9 Migrate reactor Sessions; Xapiand builds no session of its own: `docs/status/phase7c-http-wave.md`
- A5 [artifact] H1: task-based `SearchService::handle`, token cancellation, body failure, `tls_connection_factory`, phased join: `docs/status/phase7c-http-wave.md`
- A6 [artifact] 7 Test shutdown through HTTP service phases; `live_frames()` returns to baseline: `docs/status/phase7c-http-wave.md`
- A7 [artifact] Floor: Linux TSan and ASan+UBSan, macOS ASan, checks and builds; async checker 0 owed: `docs/status/phase7c-http-wave.md`
- A8 [artifact] H2: `Request::read_chunk` uses `BodyStream::read` with cancellation token; R2token and R2cancel mutants die: `docs/status/phase7c-http-wave.md`
- A9 [artifact] W2 closed: http checks loop-thread affinity; Debug workarounds deleted: `docs/status/phase7c-http-wave.md`
- A10 [artifact] W3: HTTP grace follows remaining shutdown budget; arithmetic and late-write tests kill mutants: `docs/status/phase7c-http-wave.md`
- A11 [artifact] The timing checks: `test_control_plane`'s `test_pending_keys_cost_the_loop_nothing` paces its 5,000 keys (50 every 20 ms), 6 of 6 pass beside six busy loops, mutant CPsleep fails it: `docs/status/phase7c-http-wave.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Use reactor scheduler directly; `Run` is removed and Debug counters use `engine::Counted`.
- A read/RESTORE turn wait is request-cancellable; admitted writes retain token-less budget.
- HTTP grace is `max(250 ms, (left - 5 s) / 2)`.
- Risk high: Changes cancellation, shutdown, and concurrent request lifetimes (the lead's mapping).

## May land early

None.
