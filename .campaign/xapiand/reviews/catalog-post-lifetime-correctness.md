# catalog-post-lifetime review: correctness

Commit: strata:246bb67dedccf5fd5abb2c47f8b0f0990e4c021d
Verdict: CHANGES
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The code is right; the one blocking finding is in the docs (catalog.md). A docs-only fix lands with no
new job.

## Evidence I relied on

- A1: 261119-214111-0c0179 (ASan+UBSan, 5/5 pass at strata:87ac2001) and 261119-214124-9cbf69 (5/5
  heap-use-after-free at strata:152ee0e1, test only). The command exits 1 only on 5/5 UAF and 126 on
  anything mixed, so the fails-without cannot pass on a build error. Its UAF is the gate scheduler's
  vtable post (task.hh:141), the bug's shape.
- A2: 261119-214113-02be57 is the mutant strata:c7a36046; I diffed it against strata:87ac2001: it removes
  only the hook and the `st.wait` (catalog.hh:1227-1228). 5/5 UAF.
- A3: 261119-214115-72aa8d, TSan `-R catalog`, 10/10 including catalog_post_lifetime and
  catalog_teardown (whose threaded case destroys frames while a WorkPool delivers, so it now reaches
  `posting` by timing too).
- A4: 261119-214114-d7e56a, 76/76. CI is the lead's.
- strata:246bb67 changes only docs/mechanisms/catalog.md, so these stand at the reviewed commit.

## What I checked myself

Reading, at catalog.hh (strata:246bb67) and task at the pinned task:bdafa5f1:

1. Every interleaving (Q1). A delivery meets `registered`, `suspended` or `abandoned` only (a waiter is
   taken once: `take` or `unlink_waiter`). `registered` goes to `delivered` and await_suspend's CAS
   declines (abandon cannot meet `registered`: await_suspend follows await_ready in one resume, and is
   noexcept). Abandon first: deliver sees `abandoned`, writes only `gen`/`error` into the waiter (alive by
   the list's reference) and releases the pin; it never reads `h` or `sched` (1165-1179). Abandon during
   posting: it waits, the post returns, the exchange wakes it, then it releases the pin as for
   `delivered`. Abandon after: sees `delivered`. A cancel: `~pin_awaiter` resets `cb_` first (459), which
   waits for a cancel running elsewhere, so a cancel's own delivery has ended before abandon. Inline
   resume (no scheduler): await_resume clears `w_` (486-489) before the frame can be destroyed, so no
   abandon meets it from that frame (R3 holds). After abandon returns the delivery touches only the
   waiter (`notify_all`, `release`; kept alive by the list's reference) and the catalog, never `h` or
   `sched`.
2. Deadlock (Q2). The wait spans one `sched.post`. task_asio's post is `closed()` plus `asio::post`
   (task_asio.hh:69-77): never inline, unbounded queue, its own mutex. root_list::destroy_all destroys
   each root outside `m_` (task.hh:658-672), and close/destroy_roots run on the owner's thread after the
   loops stopped (task_asio.hh:53-57), so no frame destruction holds what a post needs. Every `deliver`
   call site runs outside the shard and shutdown mutexes. A post that ran user code inline cannot wait on
   itself either: the only frame it could resume clears its `w_` first. R4 holds for task's schedulers.
3. Ordering (Q3). gen/error are written before the CAS (acq_rel); abandon's acq_rel exchange that reads
   `posting`, or its acquire wait that reads `delivered`, synchronizes with it before it reads `error`,
   `pins`, `idx`. No lost wakeup: if the delivery's exchange precedes abandon's, abandon reads `delivered`
   and never waits; if it follows, it reads `abandoned` and notifies, and `atomic::wait` rechecks the
   value. No lock was added; a suspended waiter costs one load, one CAS and one exchange instead of one
   exchange. Measured by probe 261119-222149-8acadb (below).
4. Hooks (Q4). `abandon.posting` goes through `hook_noexcept` (1059-1064), the same runtime null test of
   `host::before_cas` (361) as every existing site; nothing is compiled out, and it is inert (one null
   `std::function` test) when no hook is set, on the wait path only.
5. Docs (Q5). catalog-delivery.md's table states the states truthfully; catalog.md does not (F1).

Probe 261119-222149-8acadb (`.scratch/xapiand/review-catalog-post-lifetime-correctness/run.sh`):
deliver's old and new state transitions copied verbatim, one thread, 65536 waiters at 64-byte stride,
best of 41, with a no-op post and a mutex+ring post. The fix adds 5.7-7.6 ns per delivery to a suspended
waiter in three rounds (old 6.1-6.4 ns, new 12.0-13.7 ns with a no-op post; old 23.3-26.3 ns, new 29.0-33.1 ns
with a queue post), on x86_64 macOS under a strict hold beside 3 light holds: the extra exchange. A
delivery follows an open or a close transition, so this is noise beside it; a pin that never waits
pays nothing, and a waiter delivered before it suspended pays one load and one CAS instead of one exchange.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | catalog.md is no longer true. strata:246bb67 dropped the two preconditions that still hold: a frame dies only after its scheduler is closed (else the post the abandon waits through enqueues the dead frame's handle, and the scheduler resumes freed memory later), and a frame with no scheduler must not be destroyed while a delivery may run (its "posting" is the inline resume itself). And the bullet "`st` is decided by one exchange each side ... whichever comes second sees the other's value" plus item 2 ("if the delivery came first (`delivered`)") describe the old four-state protocol. catalog-delivery.md states neither precondition either: its only one is "a frame's destruction must not hold anything its scheduler's post needs". | docs/mechanisms/catalog.md:67-70, 76-78, 84-86 at strata:246bb67; the old text in `git diff origin/main...HEAD -- docs/mechanisms/catalog.md`; catalog.hh:1170-1175 | Restore both preconditions (phase 2 before phase 3; no-scheduler frames never race a delivery) in catalog-delivery.md's precondition paragraph, and point catalog.md's `st` bullet and item 2 at it (five states, `posting` waited through). Docs only: no job. |
| F2 | no | gap | No test covers an abandon that waits through a post carrying an error (a cancel or a failed open): that path must not unpin (`!w->error`). Straight-line code at catalog.hh:1229-1233, so not blocking. | catalog_post_lifetime_test.cc has the pin case only | A `fail` variant of the A1 test (gone_pins stays 0), as catalog_teardown's `destroyed_in_delivery(bool fail)` has. |
| F3 | no | nit | A post that destroys the frame it is given would deadlock on its own thread (abandon waits for the exchange that follows that post). No task scheduler does; the precondition is worth one clause next to R4's. | catalog.hh:1170-1175, 1225-1228; task_asio.hh:69-77 | Add "and a post never destroys the frame it posts" to the precondition. |
| F4 | no | nit | The note says "a delivery is one compare-exchange and one exchange"; it is also a load, and the diagram omits the `abandoned` → `delivered` exchange after a waited post. A2's wording ("never touches the waiter") is stricter than the code, which notifies and releases the ref-counted waiter after abandon returns; safe, and the quest's contract (no post after abandon) holds. | catalog-delivery.md:11-14, 36; catalog.hh:1175, 1180 | Reword when F1 is fixed. |

## Not run

- No ASan or TSan rebuild of my own: the contributor's jobs at strata:87ac2001 cover the reproducer, the
  mutant and the catalog tests, and the code has not changed since.
- No stress test against a sequential model for `posting`; catalog_teardown's threaded case reaches it
  only by timing.
- Xapiand's own schedulers and its test_pool were not read; R4 was checked for task's schedulers only.
- Contended cost (the resumer touching the waiter's line while the delivery exchanges) was not measured.
- CI on every platform (A4) is the lead's.
