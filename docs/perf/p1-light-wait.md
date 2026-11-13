# P1: where a light request waits under the T4/T5 mix

> Status: IN PROGRESS on explore/p1-light-wait (diagnostic branch, nothing lands). Waiting on jobs
> 261113-111450-1cdc0a (probe build), 261113-111501-0b6091 (master confirm + split), 261113-112000-4925ff (H1).

Question. Loaded light query p99 is ~45-60 ms on master (one-id landed, Xapiand:46edfb23e) against ~25 ms before
the lanes, while every HTTP loop's lag p99 stays ~4 ms. Where does a light request wait?

## Method

- Client split (Xapiand:1201b37b9, harness only): each light request records its schedule, send and end times and the
  server's `Response-Time` header (Request::begins, set at header parse, to `ends`, set when the response is emitted).
  Client latency = generator wait (schedule -> send) + server + rest (sockets, loop time before the parse and after
  `ends`, the generator reading the reply). The generator also times its own asyncio loop (1 ms sleeps) and each
  RESTORE body build.
- Server stages (Xapiand:97e3cfb58, `src/p1_probe.h`): log2-us histograms, printed by `:metrics` and diffed per phase:
  turn wait and hold per index, read refresh (`fresh`) and its refresh chore, chore queue/run/resume per class, pin
  wait, and the GET and PUT views' stages.
- Runner scripts in the worktree's `.scratch/ab/` (p1_run.sh, p1_h1.sh, p1_report.py, p1_stages.py).

## First signal (dry run, light job 261113-111213-1e18d8; not a timing)

The harness runs the light generator, 63 heavy search clients and the RESTORE stream on one Python asyncio loop.
`restore_body()` builds each 20,000-document body synchronously on that loop: 60-67 ms per chunk, and the generator's
loop stalled 65-68 ms exactly at each RESTORE start. Every light request scheduled during a stall carries it.
At 200 light/s a 65 ms stall delays ~13 requests; one chunk per 20 s phase (pre-lanes, 20,000 docs) is ~0.3% of
light requests, under p99; four chunks (80,000 docs since the lanes) is ~1.3%, over p99. That predicts the p99
step without any server-side wait.

## Hypotheses and experiments

1. H1, generator stall (job 261113-112000-4925ff): master binary, ABBA of the harness as is against
   `--restore-prebuilt 8` (bodies built before the phase, large bodies written apart from their head).
2. Server breakdown (job 261113-111501-0b6091): master vs the probe build, ABBA; which stage holds the tail
   (turn wait behind accounts' commits, read-your-writes refresh, chore queueing, loop resume).
3. Pair p50 (one-id): a new explicit id still costs two 5-shard `written_key` fan-outs (10 turns) as before one-id;
   the probe's turn and refresh stages decide whether the appended docid makes the refresh or the turn hold longer.

## Results

Pending the jobs above.
