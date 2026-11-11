# cross-shard-followups: cross-shard-id's review follow-ups: ordered tests, one catch, an assert

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status cross-shard-followups`.

## Outcome

The non-blocking findings of `reviews/cross-shard-id-correctness.md` (N1-N3) are closed; N4
(unreferenced blobs after a failed must_exist) stays in the journal as known.

## Acceptance criteria

- A1 N1: the replace_by_term and PATCH tests in `tests/test_node.cc` (around 1868, 1882-1895)
  order their race with held turns or another deterministic hook, not sleeps, and assert the
  order was reached; each still fails with its fix removed (`--fails-without`, a command that
  exits 0 only when that named test prints FAIL at a commit that builds, stderr captured).
- A2 N2: `index(did)` (src/database/handler.cc:474-479) catches doc_not_found only, so a
  commit's not_found after an applied write is never placed twice; a test that fails without it
  if one can be built, else the reasoning in the journal.
- A3 N3: must_exist without a key asserts (src/engine/strata/shard.cc:866-869).
- A4 The suite passes at the head, with CI green.

## Contracts

None.

## Decisions

Lead: normal risk; review waived if A1-A3 carry their proofs (small, test-heavy, no new
mechanism).

## May land early

None.
