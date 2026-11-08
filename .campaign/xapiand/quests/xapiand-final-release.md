# xapiand-final-release: The final Xapian-only Xapiand release

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status xapiand-final-release`.

## Outcome

The last release of Xapiand on Xapian is cut from the official `xapiand` branch (created from
v1.0.0-alpha.10, Xapiand:3fe13a2fb): the alpha.10 line plus its build fixes and the long-standing bug
fixes the owner chose to backport, tagged only after CI and the release workflows are green on the
branch. It is the X arm of the closing benchmark (D12, D23) and the release Xapiand's users stay on until
the strata release ships. Source: the owner's ruling of 2026-10-28, PLAN.md D34 ("back-port to Xapiand
the long-standing bug fixes … do a final release of Xapiand and tombstone it"), until then recorded only
in the earlier lead's memory (`xapiand-final-and-elysian.md`, `xapiand-pins-and-tags.md`).

## Acceptance criteria

- A1 queryparser replaces the vendored Xapian query parser on the `xapiand` branch, so a nested NEAR no longer overflows the heap: a regression test of the nested NEAR query under ASan, and the text adapter's comparison with Xapian's parser (`fix/xapiand-backports`, Xapiand:823a72e6d, Xapiand:ffd1c3708), in the branch's `build-test` and `asan` checks
- A2 A numeric document id 0 is no longer lost (refused, and the request takes nothing with it): the test of `explore/value-compare` (Xapiand:bdc46625d, Xapiand:b541910d6) on the branch, in the `build-test` check
- A3 Integer edge cases: a uint64 at or above 2^63 in an integer field is rejected, and float bounds on integer fields round correctly: tests in the `build-test` check
- A4 [artifact] The lossy `sortable_serialise` codec is documented as a known limit, not backported (it needs the new codec and a reindex): the branch's changelog entry for the release
- A5 CI and the Release, FreeBSD and Bottles workflows pass on the `xapiand` branch at the commit to be tagged, before the tag (the alpha.10 lesson of 2026-10-28): CI on that commit
- A6 [decision] The owner approves the tag and its version: a row in PLAN.md's Decisions

## Contracts

The `xapiand` branch's HTTP API and on-disk formats stay those of alpha.10, apart from A2 and A3's
refusals. The closing benchmark's X arm (`closing-benchmark`) builds the tagged commit.

## Decisions

- Owner, 2026-10-28 (D34): Xapian-only fixes and Xapiand release tags go on the `xapiand` branch, merged
  into master too; strata lands on master. Backports deferred until after Phase 7a ("can wait if it'll
  be costly"); 7a has passed.
- Already in alpha.10 (no work here): exact numeric ranges, exact cluster aggregations, the WAL replay
  commit, 400 for terms-only open and wide ranges. The `xapiand` branch also carries, after alpha.10,
  the bucket and nesting caps of aggregations and the url-parser overflow pin (Xapiand:387efb944,
  Xapiand:0f9e9e590).
- Open (lead): `campaign land` lands on `main`; this quest lands on the `xapiand` branch, so it is
  recorded with `campaign land xapiand-final-release --record COMMIT --reason R` once the tag exists.
- Open (owner): A2 and A3 change user-visible behavior on the old line (a reserved decision; the
  ruling names them as wanted). After the strata release names the product, Xapiand's README points to
  it and the repository is archived read-only: `release`'s, not this quest's.

## May land early

The backports one by one on the `xapiand` branch, each with its test; the tag waits for A5 and A6.
