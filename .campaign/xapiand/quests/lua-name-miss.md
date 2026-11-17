# lua-name-miss: A name-only script request that misses the cache answers an error

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
is in `state.json`: `campaign status lua-name-miss`.

## Outcome

lua-memory-bound's correctness review C1 and C2. Today a request that names a script without its
body, when the script is not cached (evicted, or over the 64 MiB byte budget), compiles an empty
script and answers ok with the document unchanged: a silent no-op older than that quest. It answers a
ClientError naming the script instead, and the docs say what bounds a large script's waiting calls.

## Acceptance criteria

- A1 A name-only request for a script that is not cached answers a 4xx naming the script and leaves
  the document unchanged; a test that fails on master (a `--fails-without` job) covers an evicted
  script and one over the byte budget (review job 261116-150120-f8089e's shape).
- A2 [artifact] `docs/mechanisms/lua-memory.md` states that calls of a script over 16 KiB wait
  for each other with no bound but the request's cancel (review C2) and the C1 error.
- A4 A schema never stores a name-only script: declaring one whose script is cached stores its body,
  so writes keep running it after a restart and on every worker thread; declaring one that is not
  cached answers a 4xx at declaration. A test covers both, plus a write through the stored schema on
  a fresh thread (review F1), and checks the status and that the document is unchanged (F4).
- A3 The suite passes (the `suite` check).

## Decisions

- Lead: the review's F1 (a stored name-only script would 400 every write after a restart) is fixed
  at declaration, so the schema carries the body; the owner was told 2026-11-16 (default: proceed).

- Lead: an error, not a silent no-op ("errors should never pass silently"); the owner was told
  2026-11-16 and did not object.

## May land early

None.
