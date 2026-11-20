# faultlab-seam: completion's four faultlab seam additions (F8)

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status faultlab-seam`.

## Outcome

completion's IO seam carries the four additions faultlab needs (design F8 and migration step 0, cpp-libraries `docs/design/faultlab.md` (cpp-libraries:727e417); PLAN D95, D96): `File::preallocate`, `File::advise` with `Advice`, `File::map_shared` returning a `SharedMapping`, and `IO::link` (no-clobber, EEXIST); plus `EntryInspection::extended_acl` and `create_exclusive`'s exact mode. Each has the default the design states, and PosixFile and PosixIO override it on Linux, macOS and FreeBSD. F9 (sync strength) is pending the owner and is not in this quest.

## Acceptance criteria

- A1 Each addition has the signature and default F8 states, and the defaults hold for a backend that overrides nothing: a test per addition.
- A2 PosixFile `preallocate` reserves space (fallocate, F_PREALLOCATE, posix_fallocate), so a later write in range cannot fail with ENOSPC; `advise` never fails; `map_shared` stores reach the file and survive `sync`: tests, each failing without its override.
- A3 `IO::link` never replaces: EEXIST on an existing name, and the source keeps its bytes: a test that fails with a `rename`-based link.
- A4 `extended_acl` and `create_exclusive`'s exact mode report and create as F8 states: tests.
- A5 The build-test check passes, and CI is green on Linux, macOS and FreeBSD, with ASan+UBSan.

## Contracts

Provides `completion:io` additions exactly as F8's code block. journal, consensus, storage and Detent's stores consume them; nothing existing changes meaning.

## Decisions

Owner D95 (one library), D96 (`link` no-clobber). F9, the sync strength, is the owner's and stays out: io.h's sync contract is unchanged here.

## May land early

None: a shared public API; it lands after its review.
