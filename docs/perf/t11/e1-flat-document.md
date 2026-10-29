# T11 E1: the strata document as flat vectors

Target: gate 7a row T11, the engine share per document (rows "replaced S1") at most 0.5x Xapian's, in CPU and on the
critical thread. Start: 0.80x CPU (0.322 against 0.404 ms/doc), 0.53x critical path (0.042 against 0.079 ms/doc).

## Footprint

Profile of one strata RESTORE of 20000 accounts documents (`harness/profile_bulk.sh`, sample(1), attribution by the frame
nearest the leaf), strata engine row 5521 samples (1 sample = 1 ms of one thread). By thread group and function:

| Group | Function | Samples |
|---|---|---:|
| DP (document build) | `StrataDocument::term_of` 1221, tree `emplace` 248, `add_posting` 160, `add_value` 42 | 1693 |
| DI (writer, under the shard lock) | `fill` 241, `~StrataDocument` 232, `normalise` 99, `~basic_document` 70 | 642 |
| AC (autocommit) | `commit_files` 483, `write_unsynced` 187, `retain` 135, `reclaim_files` 123, `~open_segment` 97 | 1147 |
| DI, prospect/strata internals | merge lambda 162, `flush_active` 157, `file_sink::put` 116, fielddict, tuple buffer | rest |

`commit_files` is mostly `fcntl(F_FULLFSYNC)` wait: the harness counts a blocked sample as CPU.

## Hypothesis

The document is two levels of `std::map<std::string, ...>`: every `add_term` pays a field-name lookup, a term-name tree
walk with string compares, a node allocation, and the destructor frees every node; `fill` then copies every term again.
A field held as a vector of terms in arrival order with an open-addressing table over cached hashes (linear scan up
to 8 terms), found from the host's field id with a name check, should remove the tree walks, the per-node allocation
and the destructor's pointer chasing. `fill` keeps the field order by name (the index sees fields as before); terms go
in arrival order, which prospect does not depend on (`valid_field` sorts its own copy). `serialise` keeps the format
(version 3) and writes terms in arrival order; it needs no sort because `unserialise` accepts any order.

## Procedure

1. Unit tests: `engine_fields`, `engine_strata`, `engine_document`, the strata fuzz decoder's fixed point.
2. Docs E2E on strata before and after (newman 6.2.3), callbacks check 0 different.
3. T11 profile: 4 interleaved base/new pairs under `quiet exclusive`, plus an A/A (base against base).

Keep if the strata engine row falls by more than the A/A spread (about 6 to 10%) and nothing else moves; remove if not.
