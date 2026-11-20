# catalog-post-lifetime: journal

State: waiting on jobs 261119-232450-2b6335 261119-232452-29e301 261119-232453-634c30 261119-232453-e232e1
Quest: .campaign/xapiand/quests/catalog-post-lifetime.md (the lead's; never edit it)

## Doing

Review CHANGES (reviews/catalog-post-lifetime-correctness.md, Xapiand:30fff2d9d) folded in at strata:765fbd3e:
- F1: catalog-delivery.md gains a Preconditions section (scheduler closed before its frames die; a
  scheduler-less frame never destroyed while a delivery may run); catalog.md's `st` bullet and abandon's item 2
  describe the five states and point at it.
- F2: the A1 test runs a second time with a failed open (gone_pins 0, slot failed, pins 0); job
  261119-225202-ee1e10 (ASan+UBSan x5).
- F3: precondition 3, a post never destroys the frame it is given.
- F4: the note names the load, the diagram has the exchange after a waited post, the contract says "touches only
  the reference-counted waiter".
Checks rerun at strata:765fbd3e (the test changed): build-test 261119-225210-db3d1f, tsan 261119-225210-192fe6.

CI linux-tsan failure at strata:b864fe08 (line 126, both variants): the test's, not the catalog's. The open
was held at `open.unlocked`, after `take()` emptied the list, so `waiters_of(0) == 1` raced it. Fixed at
strata:e3edb426: the open waits in the host's resolve (world tweak + remake) until the frame suspended. A taken
waiter is delivered by `st`, so no catalog ordering gap. Proof: TSan x50 261119-232450-2b6335; ASan x5
261119-232452-29e301; build-test 261119-232453-634c30, tsan 261119-232453-e232e1. origin/main (1ac6d65c) was
already merged at strata:b864fe08.

## Next

1. Read the three jobs; then `campaign accept catalog-post-lifetime correctness --confirm F1=... F2=261119-225202-ee1e10` is the lead's.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261119-225202-ee1e10 at strata:765fbd3e (pin and failed-open variants); 261119-214111-0c0179 (strata:87ac2001, 5/5 pass); 261119-214124-9cbf69 (fails-without at strata:152ee0e1, UAF 5/5) |
| A2 | 261119-214111-0c0179; 261119-214113-02be57 (fails-without at mutant strata:c7a36046, abandon's hook and wait removed) |
| A3 | check tsan 261119-225210-192fe6 (strata:765fbd3e); 261119-214115-72aa8d passed at strata:87ac2001 (`-R catalog` includes catalog_post_lifetime) |
| A4 | check build-test 261119-225210-db3d1f (strata:765fbd3e); 261119-214114-d7e56a passed 76/76 at strata:87ac2001; CI after the lead pushes |
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
