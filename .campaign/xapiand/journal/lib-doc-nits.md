# lib-doc-nits: journal

State: ready for review
Quest: .campaign/xapiand/quests/lib-doc-nits.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Done; awaiting review.

## Next

1. Review.

## Blocked on

Nothing

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | job 261112-032541-0728ed build-test passed at completion:a46b399 |
| A2 | [artifact] `docs/mechanisms/barriers.md` (N1, N2, N4, N5 in posix.h comment, N6) |

## Result

completion:a46b399 on quest/xapiand/lib-doc-nits: barriers.md (N1, N2, N4, N6), posix.h comment (N5) and barrier_volumes() leaked (N3). N7 untouched (lead's call). Cost numbers (4-5 ms, 44-304 images) are the review's probe, not re-measured.

## Rulings I made

None.
