> Status: CURRENT (2026-11-19) quest/xapiand/stopword-lists (D137); not landed.

# Stop-word lists: start-up, memory and schema-check cost

Probes: `build/bin/xapiand_test_stopword_lists --probe MODE` (start-up, one fresh process per sample, 30 per
cell, medians) and `--probe schema`. macOS arm64, RelWithDebInfo, strict jobs on a shared host. `files18` is a
data path holding the 18 shipped files (they are read as overrides); `none` is a data path with no stopwords
directory. `first` is the first lookup of `en`; `us` is microseconds; heap is bytes allocated in the process.

## Start-up and first use (A5)

| data path | mode | before: us / heap B / "the" stopped | after: us / heap B / "the" stopped |
|---|---|---|---|
| none | preload | 102 / 512 / no | 36 / 0 / no |
| none | first | 105 / 512 / **no** | 142 / 29,176 / yes |
| none | cold (preload + first) | 96 / 512 / **no** | 151 / 29,176 / yes |
| files18 | preload | 2,226 / 377,288 / no | 2,678 / 377,824 / no |
| files18 | first | 2,121 / 377,288 / yes | 2,586 / 377,824 / yes |

Before: job 261119-083443-f71030 (c7e5061ac). After: 261119-084202-4abafa (7b12e5d5e). A repeat of the
after probe at the head, 261119-093907-83563b (eae0478d9), on a busier host: none/first 214 us, none/cold 203
us, files18/first 4,454 us (a 30-sample range of 193-350 us and 3,700-5,200 us). The host's noise between two runs of the same code (about
1.5x for 18 files) is larger than the difference between the two builds on files18; the none/first loss is
measured twice.

Reading:

1. With no stopwords directory, "the" stopped 0 words before and stops the 570 words of `en` now. That is
   the bug fixed, and it costs 29 KB of heap (one `analysis::stopper` for the first language used, built on
   first use, not at start-up) and 40-110 us on the first use of a language.
2. Start-up itself (`preload` with no directory) gets cheaper: 102 us to 36 us, 512 B to 0 B, because
   nothing is read or allocated until a lookup.
3. With the 18 files in the data path, the loss is the fingerprint of each file (sorted, deduplicated, hashed):
   about 0.4 ms of 2.2 ms in the first pair of jobs, inside the host noise in the repeat. A node pays it once.

At 100 000 nodes or at millions of indexes per host nothing here scales with the index count: the lists load
once per process.

## Schema check on the request path

`DatabaseHandler::get_schema` runs on every request that needs the schema (index, search, info), so
`Schema::check_stop_words` runs per request. The first version walked all fields each time: 96.5 us for 200
text fields with the schema hot in cache (261119-084205-cfa578, 7b12e5d5e), 563 us when each call met a schema
copy that was not in cache (the cold row below), next to the walk `Schema`'s constructor already does per
call (`check<Error>`).

It now remembers a schema that passed with nothing to adopt (identity, `thread_local`, weak pointers), so a
request that gets the schema again pays a lookup. Job 261119-093906-670fdd (eae0478d9), 200 text fields, 2,000
distinct schema objects, 30 processes:

| call | ns per check, median | range |
|---|---|---|
| cold: first time this thread sees the schema (walk plus insert) | 562,631 | 488,389-1,542,376 |
| warm: the same schema again (lookup) | 166 | 142-343 |

So a request pays about 0.17 us instead of 96 us (hot) to 563 us (cold), 580 times less than the hot walk;
the walk is paid once per schema version and thread. A schema with fields that recorded no fingerprint (an
index from before them) is not remembered while a read-only handler holds it, and is walked each time (96 us per
200 fields) until a write adopts the fingerprints.

## Why no perfect hash

D137 asked for constexpr-phf if it fits. It builds (a 781-key set about 0.7 s of clang at -O0), but every
consumer takes `const analysis::stopper*`, the analysis library's `unordered_set<std::string>`, so any compiled
set is copied into one at first use whatever its layout. The lookup is therefore the same one as before, and
the measured start-up is not slower in a way a perfect hash could recover: with no data path the first use is
about 140-210 us and 29 KB, with a lookup cost unchanged. The compiled data is a sorted blob (50 KB in all)
whose bytes are also the fingerprint input. A view mode in `analysis::stopper` (a lookup over the blob, no
copy) would save those 29 KB and 40-110 us per language and is not worth a pinned library change for them. Dropped.

## A wide search checks every index it covers

`DatabaseHandler::get_mset` checks each distinct index of a search after the first against the stop lists it
recorded (the first one is `get_schema`'s). Probe: `xapiand_test_stopword_search --probe wide 2000 POOL`, a
search for one term over 2,000 one-document indexes, five times in one process, strict jobs, `--schema-pool-size`
POOL. Milliseconds per search; rep 0 is the first search of the process.

| run | commit | pool 100 (reps 0-4) | pool 4096 (reps 0-4) |
|---|---|---|---|
| before: only the first index checked (mutant) | quest:e4a75e7e4 | 1,645 / 858 / 935 / 894 / 965 (261119-112243-6bdb1d) | 2,386 / 2,865 / 3,419 / 971 / 1,027 (261119-112245-25a678) |
| after, every index through the schema cache | quest:0772ade92 | 24,283 / 29,155 / 16,981 / 6,720 / 3,022 (261119-112248-434c12) | 1,254 / 4,693 / 1,011 / 760 / 737 (261119-112250-23f8dd) |
| after, agreeing indexes remembered | quest:18108e527 | 1,531 / 2,715 / 610 / 560 / 572 (261119-121016-295597) | 645 / 584 / 576 / 561 / 613 (261119-121017-3c611b) |

The uncached check cost 3-18 times more at pool 100 (reps 2-4: 3,022-16,981 ms against 894-965): 2,000
schemas through 100 slots are evicted and reloaded from metadata on every search. At pool 4096 every schema
stays cached and the check cost nothing measurable (737-1,011 against 971-1,027). With the verdict remembered
by path, steady state is 560-610 ms at both pool sizes, and the first search of a pool-100 process pays the
one-time schema loads (rep 1, 2,715 ms), so read reps 2-4.

Honest limits: the "before" commit is the mutant (quest:e4a75e7e4: `0772ade92` with the check removed), not
the same code with the check off, and it ran in a different strict window from the others; the pool 4096 "before"
reps 0-2 (2,386-3,419 ms) are noise of that window, not a cost of the code. The 560-610 ms steady state is below
the 894-965 ms of the mutant; nothing in the change makes a search faster, so that gap is the runs' windows,
not a speedup. The claim rests on pool 100 against pool 4096 at the same commit.
