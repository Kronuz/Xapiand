# bulk-load-rerun: the 2018 bulk loads rerun: Xapiand on strata against Elasticsearch today

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status bulk-load-rerun`.

## Outcome

In April 2018 German bulk-loaded Kibana's three tutorial samples into Elasticsearch (`_bulk`) and
Xapiand on Xapian (`:restore`) on one Mac: Elasticsearch took 7.90 s, 11.29 s and 0.08 s for
shakespeare (111,396 docs), logs (14,005) and accounts (1,000); Xapiand took 15.09 s, 22.61 s and
0.21 s, about 2x slower. The same files are in `~/Development/Kronuz/datasets/kibana-2018/`
(zstd; README.md there). This quest reruns those loads on this machine today: Xapiand master on
strata (`:restore` of `es-*.msgpack`) against a current Elasticsearch release run from its
tarball with its bundled JDK (no Docker), one shard, no replicas, defaults otherwise, each load
timed to its durable acknowledgment (Xapiand's commit; Elasticsearch `refresh=wait_for` or a
following `_refresh`, stated). `docs/perf/bulk-load-rerun.md` reports docs/s per engine and dataset
beside the 2018 column, and peak RSS and on-disk size after load.

## Acceptance criteria

- A1 A script `harness/bench/bulk_load.sh` downloads nothing it does not pin (the Elasticsearch
  version and its SHA-512), starts each engine fresh, loads each dataset 5 times and prints the
  median and spread: a dry run job at 1 trial
- A2 [artifact] `docs/perf/bulk-load-rerun.md`: the table (2018 seconds, today's Xapiand and
  Elasticsearch medians with spread, docs/s, peak RSS, size on disk) from `exclusive` jobs, each
  row naming its job id, and what moved since 2018 in one paragraph with numbers

## Contracts

None.

## Decisions

- Lead, 2026-11-20: Elasticsearch runs from its official tarball on this host (no Docker, no AWS);
  the tarball is removed after the run. Timings are `exclusive` jobs.

## May land early

Everything: a benchmark and a note.

## Tasks

None: one contributor (exploration, economical, medium).
