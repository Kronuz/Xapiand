# restore-stream review: security

Commit: Xapiand:5842f9ef3
Verdict: APPROVE
Date: 2026-11-21

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261121-034148-685f8b (5842f9ef3): feed and node tests, 1200 checks with values, including the D162
  rows (fewer, more, toodeep, invalid, trailing, twoarrays) answering errors end to end. These are enough
  for "every error path answers an error status".
- 261121-034201-4acbcc, 261121-034203-69a143: the A4 tests fail at the prototype.
- 261121-020737-3fde05 (a131ad2b2; src/ is identical at 5842f9ef3): the A6 fuzz, 4 x 480 s under
  ASan+UBSan, about 10.6M bodies, no violation.

## What I checked myself

Probe job 261121-041010-ff1b0b at 5842f9ef3, ASan+UBSan (the A6 job's configuration), exit 0, 0 failures.
Script: `.scratch/xapiand/review-restore-stream-security/probe.sh` builds `test_review_rs_probe.cc` into the
job checkout and removes it afterwards. The probe measures the heap peak with sanitizer malloc hooks.

1. Declared count (risk 1). `array_left` is only a counter (search_views.cc:3016-3027). Nothing reserves or
   loops by it. P4 declares 2^32-1, then feeds 200,000 elements byte by byte: linear (2.8 s under ASan),
   and the end fails as "ended inside". Nested headers declaring 2^32-1 items cost 0.07 MiB (P2a, P2b).
   Pinned Kronuz/msgpack 1e9aaaf reserves by the bytes that follow, not by the count (unpack.hpp:170-197, 436-453).
2. Read boundaries (risk 2). P3 uses a 1,040,399-byte array: three 300 KB str32 values, plus bin32,
   ext32, fixext, float32, uint64 and str16. It is cut into pieces of 1, 2, 3, 5, 7, 4096 and 65536
   bytes, and in mixed pieces. Every run equals the whole decode, and ASan stays clean. All 4096
   truncations in the last 4 KiB fail. A trailing byte fails at every cut near the end. An element is
   never half-applied: it reaches `objects` only once complete (3049-3070). The A5 tests cover header
   splits at every offset.
3. Error paths and keep-alive (risk 3). Every new throw is a ClientError (3032, 3062, 3073, 3089). The view
   commits what was applied and rethrows (1802-1813), which answers 400 as job 685f8b shows. When the
   handler ends before the body is fed, http_lib eb08b59 drops keep-alive and closes the connection
   (http_asio.h:1029). A body read in full is framed by Content-Length in the parser. The rest of a
   body therefore cannot become the next request.
4. Memory (risk 4): see F1.
5. Fuzz reach (risk 5): see F2.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | defect | Elements cost memory per element, not per byte. A chunk's object handles are all held until the chunk ends, and each owns a fresh 8 KiB msgpack zone (`release_zone` allocates `new zone`, unpack.hpp:1406-1418). A 64 KiB read of one-byte elements peaks at 521.6 MiB, while master's whole-array decode of the same body peaks at 4.2 MiB. Master's stream-of-objects path already costs 536.1 MiB for the same bytes, kept in `objects` (3081; each MsgPack owns its own zone), so an attacker gains nothing new. The claim "bounded by the largest element" holds only for dump-shaped data; the A1 datasets have about 17 elements per read. | search_views.cc:3049-3070; probe 261121-041010-ff1b0b P1a/P1b/P1c/P1e | Owner's call. Option (a): append each element to one chunk MsgPack as it is parsed, so one handle is live at a time. A test would then bound the peak for 64 KiB of `0x80`. Option (b): keep it, and file the pre-existing per-object zone cost of the stream path on the hunt list. |
| F2 | no | gap | The A6 fuzz cannot reach several paths. First, elements above the unpacker's 64 KiB initial buffer, so it never exercises buffer expansion with referenced strings across a cut: strings are at most 40 B, bins at most 20 B, and bodies are tiny. Second, ext, float32, str8 and wider, and bin16 and wider types. Third, allocation accounting. Fourth, parse_ahead, next_batch and the view's error mapping. Fifth, the `as_before` comparison whenever master's unpacker threw. This probe covered the first two under ASan. Tests cover the fourth. | tests/test_restore_stream_fuzz.cc `value`, `element`, `main` | Optional: one large-element body shape and ext values in the generator. |
| F3 | no | nit | Elements applied before an error stay committed. That is D162 as ruled and matches NDJSON. A client sending a short array now gets partial data plus a 400, where master restored nothing. | restore_database_view 1802-1809; job 685f8b `fewer` | None (owner may reverse D162). |

## Not run

- No end-to-end keep-alive probe with a pipelined second request; risk 3 rests on http_asio.h:1029 and Content-Length framing.
- No RSS run of an adversarial dump through the node: the write buffer held until the one commit grows with the body. That is the same for NDJSON and is stated in A7.
- No platform other than macOS arm64/llvm 22; other platforms are CI's.
