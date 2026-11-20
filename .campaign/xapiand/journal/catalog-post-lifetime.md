# catalog-post-lifetime: journal

State: ready for review (all five jobs passed; lead fixed the stale catalog.md section at strata:246bb67de, 2026-11-19 22h).
Quest: .campaign/xapiand/quests/catalog-post-lifetime.md (the lead's; never edit it)

## Doing

Jobs queued at strata:87ac2001 (fix), strata:152ee0e1 (test only, unfixed catalog) and the A2 mutant
strata:c7a36046 (off-branch, kept by refs/mutants/catalog-post-lifetime/no-wait in the strata repo).

## Next

1. Read the five jobs; if the unfixed or mutant job is invalid (126: mixed or non-UAF failures), read its log.
2. Paste `campaign brief`'s table here; State to ready for review (correctness lens); the lead pushes for CI (A4).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261119-214111-0c0179 (fix, ASan+UBSan, 5/5 pass); 261119-214124-9cbf69 (fails-without at strata:152ee0e1, UAF 5/5) |
| A2 | 261119-214111-0c0179; 261119-214113-02be57 (fails-without at mutant strata:c7a36046, abandon's hook and wait removed) |
| A3 | check tsan 261119-214115-72aa8d (`-R catalog` includes catalog_post_lifetime) |
| A4 | check build-test 261119-214114-d7e56a; CI after the lead pushes |
| A5 | [artifact] `docs/mechanisms/catalog-delivery.md` (strata:87ac2001) |

The ASan job's command exits 0 only if 5/5 runs pass, 1 only if 5/5 report heap-use-after-free, else 126
(invalid), so a fails-without job cannot pass on a build error or an unrelated failure.

## Result

strata `include/strata/core/catalog.hh`: a waiter gains a `posting` state between `suspended` and
`delivered`. `deliver` moves a suspended waiter to `posting` (compare-exchange loop: `registered` goes to
`delivered`, `abandoned` stays), posts, then exchanges `delivered` and notifies only if it met `abandoned`.
`abandon` exchanges `abandoned`; on `posting` it calls hook `abandon.posting` and `st.wait`s until the
delivery's exchange, then releases the pin as for `delivered`. No lock on the hot path: one load, one CAS and
one exchange per suspended waiter instead of one exchange.

Test `test/catalog_post_lifetime_test.cc` (ctest `catalog_post_lifetime`): the open delivers on a WorkPool
thread; hook `open.unlocked` holds it until the frame suspended; the gate scheduler's post holds the delivery
while the test thread closes the scheduler, destroys the frame, destroys the scheduler. The gate releases on
`abandon.posting` (fixed) or after the scheduler's destruction (unfixed: the post reads freed memory). No
sleeps or timeouts. Note: `docs/mechanisms/catalog-delivery.md`.

Left open: `docs/mechanisms/catalog.md` "Waiters whose frames die" still says "st is decided by one exchange
each side" and that the post comes after the frame is gone; it is outside this quest's file set, so the lead
should point it at catalog-delivery.md.

## Rulings I made

- R1 The test's handshake uses a new hook site `abandon.posting` in `abandon`'s waiting branch (the
  `before_cas` hook, as `cancel` and `open.unlocked`), so the fixed run needs no timeout. Cost: a null
  std::function test on the rare waiting path only.
- R2 The A2 mutant removes the hook and the wait together; a mutant keeping the hook would release the gate
  early and make the race timing-dependent rather than deterministic.
- R3 An inline resume (no scheduler) also runs under `posting`; no abandon can meet it (await_resume clears
  the awaiter's waiter first), so it changes nothing observable.
- R4 Precondition stated in the note: a frame's destruction must not hold anything its scheduler's `post`
  needs (else abandon would deadlock). task's schedulers destroy roots outside their locks and no post
  destroys a frame (task.hh `destroy_all`, task_asio.hh `post`).
