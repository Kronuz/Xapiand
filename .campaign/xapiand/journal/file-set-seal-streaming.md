# file-set-seal-streaming: journal

State: waiting on jobs 261124-004325-148d8f 261124-004328-8df38e
Quest: .campaign/xapiand/quests/file-set-seal-streaming.md (the lead's; never edit it)

## Doing

Jobs queued at strata:7a4284c4 (fix) and strata:213561d9 (test only); branch pushed for CI (A4).

## Next

1. On green: paste `campaign brief` Evidence table, State "ready for review".

## Blocked on

Nothing.

## Evidence

Commits: strata:213561d9 adds the test alone (seal still a whole read), strata:7a4284c4 the fix.

- 261124-004323-a98c76 (A2): `group_file_set_test` at strata:7a4284c4, passed (exit 0).
- 261124-004325-148d8f (A2, fails-without): the same test at strata:213561d9; exits 1 only when it
  fails on "a seal reads at most", 126 on a build error or any other failure.
- 261124-004328-8df38e (A1): build-test.

A3 (`git grep -n "crc32c\|read(" -- include/strata/group/file_set.hh` at strata:7a4284c4):

```
include/strata/group/file_set.hh:12:#include <crc32c/crc32c.hh>
include/strata/group/file_set.hh:51:	std::string read(std::string_view name, std::uint64_t offset, std::size_t n) {
include/strata/group/file_set.hh:71:		it->second.crc = crc32c_of(*open(name, false), it->second.length);
include/strata/group/file_set.hh:132:			if (f.crc && crc32c_of(*file, f.length) != *f.crc) { throw file_set_damage("strata::group: a sealed file fails its CRC32C: " + name); }
include/strata/group/file_set.hh:152:	static std::uint32_t crc32c_of(kronuz::journal::File& file, std::uint64_t length) {
```

`seal` (:71) and recovery (:132) both call `crc32c_of`; the only `read` left (:51) is the apply
reader callers use for bytes, not checksums.

| Criterion | Evidence |
|---|---|
| A1 | pending 261124-004328-8df38e |
| A2 | 261124-004323-a98c76 passed; fails-without 261124-004325-148d8f pending |
| A3 | git grep above |
| A4 | pending CI at 7a4284c4 |

## Result

`file_set::seal` now checksums with `crc32c_of`, the 64 KiB streaming reader recovery verifies
with, instead of `crc32c(read(name, 0, length))`: sealing a volume allocates 64 KiB rather than the
volume's length (up to 32 GiB for a rotated volume), and the CRC32C value is unchanged (the test
compares it with the whole bytes' CRC32C). Test: `test/group/file_set_test.cc`, a `bounded_io`
over faultfs recording the largest read; a 1 MiB + 7 file and an empty file seal to their whole
CRC32C with no read over 64 KiB, and recovery reads within the same bound. How it works: the
header comment at `include/strata/group/file_set.hh:66`; `docs/mechanisms/replication-group.md`
already describes sealing without the read strategy and stays true.

## Rulings I made

- Bound measured as the largest `read_at` span asked of the file, not by capping allocations: that
  is the IO read-size bound A2 names and it catches any whole-file read; an operator-new cap would
  need a sanitizer skip as footprint_test does.
- The fails-without job runs the plain test at the test-only commit strata:213561d9 (the unfixed
  seal), not a mutant: `test/group/mutants.py` is outside my file set.
- No docs change: `docs/` is outside my file set, and the mechanism doc does not state how seal reads.
