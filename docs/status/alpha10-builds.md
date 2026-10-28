# alpha10-builds

State: branch `fix/alpha10-builds` (on `origin/xapiand`); CI green on every job at the last push before the final commit; the Release, FreeBSD and Bottles workflows are not run on the branch (they publish).

Done:
- xapian.h include path for XAPIAN_OBJ; compressors pin 816bc82 (clash fix compressors:9d2a87c).
- FindSanitizers: `-fno-sanitize=function` only for Clang.
- WAL task members initialised; AggregationMatchSpy::unserialise argument order; test_pool local node; test_engine_sort build and the two random-bytes tests skipped when asserts are on (sortable_unserialise asserts on non-number bytes).
- XAPIAN_ASSERTS option (the vendored Xapian's assertions; on in the CI assert job and in Debug).
  - Our bug, fixed: `prepare_mset` on an empty query asserts `!query.empty()`; search.cc now skips it.
  - Xapian's own bug, public API only: after the highest docid is erased, `postlist_begin("").skip_to(d)` for d >= that docid asserts `old_termfreq == termfreq` (glass_postlist.cc:925). test_engine_read skips those probes when Xapian assertions are on.
- tls_check: s_client for the untrusted-CA refusal (curl 8.18 on the runners finishes the handshake and rejects afterwards), log polling up to 6 s.

Host: Docker Desktop memory was raised from 2 to 8 GiB for the Linux repro builds. Done with it: the `alpha10` container and volume removed, memory setting restored to 2 GiB, Docker Desktop quit.

Next: lead merges into `xapiand`, then master.
Blocked on: nothing.
