# task-inbox-starvation: a self-reposting loop cannot starve a cross-thread post

Risk: high (concurrency). The lead writes this quest; contributors read it and never edit it. Its
machine state is in `state.json`: `campaign status task-inbox-starvation`.

## Outcome

task-lifecycle's re-check (reviews/task-lifecycle-correctness.md, Re-check at task:27028a0):
R1, `inline_scheduler` runs a cross-thread `post_call` only after every demoted handle, so a loop
that keeps re-posting itself starves it (task.hh:447; probe 261119-091929-77b81e: posted at step
10, ran at step 10,000); the inbox is taken before demoted handles. R2, an `off_loop` thread
start failure skips the end flush (lifecycle.hh:448-458, 522-526); it flushes inline. R4, the
relaxed owner-thread read in `post_call` (task.hh:402) is ordered or shown safe. N8: `event::wait`
leaves service-shape.md:291.

## Acceptance criteria

- A1 A cross-thread post made while a loop re-posts itself runs within a stated bound of steps,
  counted by a test with values; a mutant that takes the inbox after demoted handles fails
  (`--fails-without`)
- A2 A failed `off_loop` thread start still flushes at the end: a test that injects the failure,
  with a mutant (`--fails-without`)
- A3 The suite and TSan pass: the suite and tsan jobs
- A4 [artifact] R4's ordering argument or fix is in the journal; service-shape.md no longer names
  `event::wait`
- A5 [ci] CI is green on every platform

## Contracts

Provides task:scheduler (behavior only; no API change).

## May land early

Nothing: one landing.
