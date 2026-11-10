# cjk-segmenter review: correctness

Commit: analysis:229b68ce9aca5755d714541b17430536da9a5b22
Verdict: APPROVE
Date: 2026-11-09

APPROVE: lands as is, at this commit's code. CHANGES: lands after each blocking finding is fixed
and confirmed by a job at the fixed commit. REJECT: the approach is wrong; the quest is re-planned.
An APPROVE lists no blocking finding; a CHANGES lists at least one.

## Evidence I relied on

The contributor's jobs ran at analysis:3e55476. analysis:229b68c adds only 15 lines to
`docs/cjk-segmenter.md` (`git diff --stat 3e55476 229b68c`), so they stand for this commit.

- A1: build-test 261109-155853-9a5466 and mutants 261109-155858-8c7696. I read the log: 12 of 12
  killed. Each mutant is a real `sed` against `cjk/segmenter.cc`, and the script reports a
  mutant that is not applied.
- A2: 261109-155854-677180. I read the log, which answers risk 1:
  - The oracle is `/usr/local/opt/icu4c@78`, found as "78.3", and every mode prints "ICU 78.3".
  - zh, ja and ko each show 0 differences, and the comparison covers kinds as well as offsets
    (`boundary::operator==`).
  - `dict` reports ICU's compiled `cjdict.dict` = `data/cjdict.txt` (315,964 words), and our
    FST disagrees on 0.
  - The sweep reports 0 differences outside Complex_Context scripts.
- A4: fuzz 261109-155855-c5bbf7: ASan+UBSan, library instrumented, 145,397 runs in 301 s,
  `-max_len=8192`.

Like with like (risk 1):

- **Rules:** `cjk/tables.hh` is ICU 78.3's own compiled state machine, regenerated identical by
  my probe below, so rules and data match the oracle's ICU by construction.
- **Normalization:** ICU's NFKC is reimplemented from ICU's own `Normalizer2` data. The sweep
  runs every code point alone and doubled in 14 contexts, which covers every engine
  character's decomposition, plus the halfwidth voicing compositions (`ｶ … ﾟ`, `… ﾞ`).
- **Dictionary path:** the corpora drive it hard: 54,208 zh words and 65,531 ja words, all
  equal, from real text through the least-cost search, the unknown-character fallback and
  the katakana heuristic. The sweep alone is mostly short inputs. The corpora and the 3,035
  vectors (including katakana runs around 20 and a cost tie) are what reach the lookup and
  fallback paths, and they agree.

## What I checked myself

Scripts are in `/Users/kronuz/code/Kronuz/Xapiand/.scratch/review-cjk-segmenter-correctness/`.
Both jobs ran at 229b68c with `--env PROBE=c8ff7e714edf`.

- **261109-200756-3b24bf** (`perf.sh` + `probe_perf.cc`, exit 0), risk 2:
  - 34 adversarial shapes at doubling sizes up to 262,144 code points (about 1 MB), in a
    Release build. Shapes:
    - Empty input.
    - Dictionary Han; Han with no dictionary match (BMP cycle, SMP Ext-B); one repeated Han
      character.
    - Katakana runs, `ー` runs, halfwidth voicing.
    - Han followed by 131k `ﾞ` (one dictionary run).
    - Non-starter reordering blocks: `ﾞ` (ccc 8) against U+16FF0 (ccc 6), the one place an
      insertion sort could go quadratic.
    - Compatibility ideographs, squared katakana, `ゟ`.
    - Hangul, Han/Hangul alternating, Thai.
    - Lone combining marks, Han + 262k U+0301, `a.` + 262k Extend (rule backtracking),
      MidNum + Extend.
    - ZWJ runs, regional indicators, Hebrew quotes.
    - Random invalid bytes, encoded surrogates, sequences above U+10FFFF, truncated and
      overlong sequences, and Han ending in a truncated sequence.

  Every shape is linear: ns/byte stays flat from 4k to 262k. The worst is squared katakana at
  391 ns/byte, and the longest single run took 0.31 s at 1 MB. The reordering case never
  forms a long fragment, because the rules break between the marks (131,073 segments).
  Every output tiles the text, and a reused segmenter gives identical segments.

  The same shapes then ran up to 65,536 code points in a separate ASan+UBSan build (every
  target instrumented, `-fno-sanitize-recover=all`). After that, 300 random texts of up to
  1 MiB mixed every class with raw invalid bytes, repeated runs of up to 300, and cut
  sequences. No sanitizer report, all tiled, reuse equal.
- **261109-200756-110fb5** (`regen.sh` + `probe_icu.cc`, exit 0), risks 1, 3 and 4:
  - `analysis_gen_tables` against ICU 78.3 regenerates `cjk/tables.hh` IDENTICAL.
  - `icu_compare vectors … 3000` and `lines` regenerate `test/vectors/cjk.jsonl` and
    `cjk-corpora.txt` IDENTICAL.
  - `data/cjdict.txt` has sha256 `e73fd720…6663a0`, as the doc states. Its longest word is 16
    UTF-16 units, and 0 words reach 19 units. There are 23 single-katakana words, with a
    maximum cost of 83, and all costs run from 27 to 251.

  The data confirms both equivalent-mutant claims of ruling 9. Max word units 20 to 19
  cannot matter. A 20-katakana run at 8,192 always loses to at most 20 × 255 = 5,100.
- **Complex_Context next to ICU** (same job), risk 4:
  - ICU divides Thai and Lao sentences into words: `สวัสดี|ครับ`, `ภาษา|ไทย|เป็น|…`,
    `ພາສາ|ລາວ`. Ours keeps each run whole, as ruling 2 says.
  - A Thai run inside a Han segment is cut where ICU cuts it: `日本|ไทย|語`, same as ICU.
  - On Khmer and Myanmar words of this length, ICU agrees with ours.
- Licence (risk 3): I diffed `LICENSE-ICU` against `/usr/local/opt/icu4c@78/LICENSE`. Lines
  1–42 (Unicode-3.0) and 90–292 (cjdict: Google BSD, libtabe, IPADIC) are verbatim apart from
  separator lines. README's License section names Unicode-3.0, the BSD dictionary licence,
  libtabe and IPADIC, and links `LICENSE-ICU`.
- Code read: `cjk/segmenter.cc`.
  - `class_top` has 2,176 entries = 0x110000 >> 9, and `unicode::next_utf8` never yields
    more than U+10FFFF, so every table lookup is in bounds.
  - The per-position match arrays hold `max_word_units + 1` = 21 entries, against at most
    20 finals plus one unknown.
  - Every reached position has `prev_` set, so the backtrack ends at 0.
  - Katakana and unreached-skip order follow ICU's `divideUpDictionaryRange`.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | gap | Ruling 2 (Complex_Context runs left whole) is sound for the consumer: it is what `term_generator` does for Thai today, and the quest names only zh, ja and ko. It is documented in the README and in `docs/cjk-segmenter.md` ("Where it differs"). But no test pins the chosen behaviour on text ICU divides: the only Thai case, `กขค` in `test/vectors/cjk-cases.txt:14`, is one ICU also keeps whole. Also, the sweep's "868 in Complex_Context scripts" reads as a size of the divergence, while real Thai or Lao text differs at nearly every word boundary. | job 261109-200756-110fb5 (`สวัสดีครับ`, `ภาษาไทย…`, `ພາສາລາວ`, `東京タワーในกรุงเทพ`: ICU divides, ours whole) | Add a hand case asserting a Thai sentence stays one segment (and Han/Thai/Han cuts at the script change). Say in the doc that the sweep count is not a measure of Thai divergence. Owner's options: keep whole runs (status quo), or add a Thai engine later as its own quest. |
| F2 | no | nit | The header says invalid UTF-8 "read[s] as ISO-8859-1, one at a time". Encoded surrogates (`ED A0 80`) instead decode as one surrogate code point, because `unicode::next_utf8` checks only `c >= 0x800` for 3-byte forms. This is memory-safe, and the class table covers U+D800–DFFF, but the stated contract is inexact. | `cjk/segmenter.hh:55-56`; job 261109-200756-3b24bf `surrogates`: 262,146 bytes give 87,382 segments, one per triple | Either reject surrogates in Kronuz/unicode's `next_utf8` (shared with `term_generator`, so a library change) or word the comment as "ill-formed sequences other than encoded surrogates". |
| F3 | no | nit | `LICENSE-ICU` quotes ICU's Unicode-3.0 and cjdict notices but not the "ICU License - ICU 1.8.1 to ICU 57.1" section of the same file. `data/cjdict.txt` itself carries "Copyright (C) 2012-2015, Google Inc, International Business Machines Corporation", from that era. A3's named licences are all present; this is a conservative addition. | `data/cjdict.txt:4`; `/usr/local/opt/icu4c@78/LICENSE` lines 50-81 | Append that section verbatim to `LICENSE-ICU`. |
| F4 | no | nit | One dictionary run costs about 40 bytes of buffers per character. `prev_` is `int32_t`, so a single run over 2^31 characters would truncate the backtrack. Not reachable in practice (6 GB of Han with no rule break), and memory stays linear. | `cjk/segmenter.hh:97`, `cjk/segmenter.cc` backtrack loop | None needed. A size-typed `prev_` removes the edge if wanted. |

## Not run

- Linux, FreeBSD, arm64 and other compilers: CI covers them. My probes are x86_64 macOS, Apple
  clang.
- I did not compare `data/cjdict.txt` byte for byte with ICU 78.3's source tarball (not on
  this machine). Content equality with ICU's compiled `cjdict.dict` is established by A2's
  `dict` mode, and the sha256 matches the doc.
- No fuzzing beyond A4's 5 minutes. The 300 random texts of up to 1 MiB under ASan+UBSan
  cover lengths beyond libFuzzer's 8,192.
- A5 throughput was not re-measured (performance is not this lens). `tools/fetch_corpora.py`
  was not rerun (network).
