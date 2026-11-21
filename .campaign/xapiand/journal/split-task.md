# split-task: journal

State: waiting on jobs 261121-122744-c21719 (A1) 261121-122744-09cab8 (A2) 261121-122752-7d2bfd 261121-122757-6d4caf (A3) 261121-122809-473c08 261121-122803-c360df (A6)
Quest: .campaign/xapiand/quests/split-task.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Split pushed at task:259f0cb6d73f (quest/xapiand/split-task): task.hh, test/asio_test.cc, test/sync_test.cc,
one commit each, plus docs/source-layout.md. Jobs queued; none has run.

## Next

1. Read each job's result; fix a build error or a self-containment failure if one shows.
2. A3: diff the two `ctest -N` logs (base 261121-122757-6d4caf, head 261121-122752-7d2bfd).
3. A2 exits 2 until tools/split_check.py lands (quest split-check); rerun then and answer its warnings.

## Blocked on

Nothing. A2 waits for split-check.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-122744-c21719 (queued) |
| A2 | split 261121-122744-09cab8 (queued; exit 2 until tools/split_check.py lands) |
| A3 | ctest -N: base 261121-122757-6d4caf, head 261121-122752-7d2bfd (queued; 261121-122751-bf620d was superseded by the head job despite its own --slot, resubmitted) |
| A4 | docs/source-layout.md at task:259f0cb6d73f |
| A5 | CI on quest/xapiand/split-task (pushed) |
| A6 | selfcontained over task/*.hh plus the two test support headers: 261121-122809-473c08 (261121-122803-c360df is the same command run by `sh`, redundant) |
| A7 | lead's reading |

## Result

task.hh (2,182 lines) is now an umbrella over seven headers in `task/` (scheduler, drivers, frame_pool,
coroutine, cancel, timing, sync), cut at the file's own section headings (cancel and timing at the
end of `cancel_callback`, 380 and 257 lines). File order is dependency order, so each piece includes
the one before it. test/asio_test.cc (1,747) is main plus four topic files and a support header;
test/sync_test.cc (1,075) is main plus one topic file and a support header. All pieces are 100 to 570
lines; no file is kept whole over 1,000 lines. Layout in docs/source-layout.md. AGENTS.md's repo map still
reads "task.hh holds ..." and is outside this quest's file set: lead may want one line there.

## Rulings I made

- Split task.hh despite being the library's one core header: every section was already named in the
  file, and the pieces chain, so users still include `task.hh` only. Alternative (keep whole) rejected
  because cancel/lock/when_all are unrelated to schedulers when someone edits them.
- Non-moves, for A2: (1) `static int failures = 0;` in asio_test.cc and sync_test.cc became
  `int failures = 0;` (lost `static`) with `extern int failures;` in each support header, so all
  pieces share one counter. (2) Prototypes added in the support headers for functions defined in one
  piece and used in another: demote_once, spin, wait_for, plain_waiter, the test_* entry points.
  (3) The TASK_ASAN/TASK_POISON macro block moved from before `namespace task` to frame_pool.hh (its
  only user). (4) The `operator new` replacement and `allocations` stay together in asio_test.cc.
- No dynamic-initializer namespace-scope object moved apart from what it depends on (inline thread_locals
  and `parked_handle` stay with their users).
- Nothing was compiled by me; only jobs. Statement counts and inlining: task.hh pieces are one TU's
  text reassembled by the umbrella, so the preprocessed output is the same code in the same order.
