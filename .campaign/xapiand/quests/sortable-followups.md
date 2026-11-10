# sortable-followups: sortable-proof review nits S1, S3, S4

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status sortable-followups`.

## Outcome

The three non-blocking findings of `reviews/sortable-proof-security.md` are closed in Kronuz/sortable
without changing a byte the library encodes or a reply the CLI gives. The mechanism note says what is
true of a hand-built `number` (it converts without undefined behavior, but may give values no `decode`
produces). The fuzz canonicality oracle also checks the halves `a` and `b`, and the corpus has seeds
that are not split-prefixed, so accepted inputs are reached sooner. The CLI's lenient forms are
documented as accepted.

## Acceptance criteria

- A1 [artifact] S1: `docs/mechanisms/sortable.md` no longer says an out-of-range exponent can overflow `to_double`, and states that every `number` converts without undefined behavior: a quote of the new sentence.
- A2 S3: the fuzzer runs `reencode` on both halves as well as the whole input, and the seed corpus gains unprefixed seeds: a light job building the fuzzer with ASan+UBSan and running it for at least 10 minutes from the committed corpus, plus D5.9's 2^1024 mutant (job 261109-163356-a7173d's method) found in less time than its 60 s, the before and after times in the journal.
- A3 [artifact] S4: the CLI protocol text in the docs or README lists leading zeros, `i -0` and uppercase hex as accepted: a quote. The CLI itself does not change (narrowing it is the owner's call; no change asked).
- A4 the build-test and asan checks pass at the landed code.

## Contracts

None changed: `sortable.hh` and `tools/cli.cc` are outside the file set.

## Decisions

- Lead, 11-09: S4 documented, not tightened, since tightening narrows accepted input (the owner's call; not asked for).

## May land early

Nothing; it is one small landing.
