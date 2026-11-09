# Closing benchmark: the local baseline of 2026-11-04

> Status: HISTORY (2026-11-04) a smoke-layout run of the closing harness on the local Linux VM; not a
> published or gate measurement.

The closing harness of `fix/closing-bench-harness` at Xapiand:ec4e6d937 ran every row it has (T1, T2, T4a, T4b,
T5a, T5b, T6, T7, five rounds each, plus the B1 and B2 builds) at 200,000 documents of C-wiki and C-logs, with
Xapiand on strata (S, and S' as its A/A copy) against the Xapian-only `xapiand` branch (X), Elasticsearch (E)
and Quickwit (Q), on the local QEMU Linux VM (8 vCPU, 16 GiB, `host-identity.txt`) in the harness's smoke layout
(2 system cores, 4 GiB per arm), the rounds under one `quiet exclusive` hold on 2026-11-04 from 19:39 to 22:39
PST. Every step exited 0 (`steps.tsv`). `run.sh` is the driver
(`~/code/Kronuz/Detent/.scratch/overnight/done/Xapiand-baseline.sh` on the laptop, queued by the lead's "Write
that full local baseline" on 2026-11-04); the raw rows stay in `~/.scratch/xapiand-baseline/20261105T033044Z/`
on the laptop (5.3 MB). `report.csv` is `closing_report.py report` over them: per metric, the anchor (the best
arm in rounds 1 and 2), the D11 verdict of S against it, the median ratio as a cost (above 1, S is behind) with
its interval, the A/A deviation, the pairs, and the verdict against X (`docs/mechanisms/closing-harness.md` on
the harness branch defines each column).

What it shows, at this size and on this VM only:

| Row | Against the anchor | Against X |
|---|---|---|
| T1, T2 indexing | S behind E (T1, 3.47x on documents/s) and Q (T2, 12.2x) | not distinguishable |
| T4a term, phrase, and/or queries | not distinguishable, except `or/search` 1.17x worse than E | not distinguishable |
| T4b aggregations, ranges, sorts | worse than E or Q on every metric: `agg_histogram` 71.6x, `agg_terms` 14.2x, `agg_stats` 9.9x, ranges 6.5x to 9.8x, sorts 1.8x to 8.1x | better or not distinguishable |
| T11 idle memory (in T4b) | anchor X | **1.28x worse: REGRESSION** |
| T6 search and write latency | worse than E (p50 10.1x, p99 32.3x) and Q (write ack p99 267x) | not distinguishable |
| T7 restarts | crash restart 1.39x worse than Q | not distinguishable |

It proves the harness runs end to end without AWS. Its ratios are smoke values: 200,000 documents, one VM with
shared host cores, not the design's metal host or sizes; T5b has no pairs for E.
