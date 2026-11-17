# platform-rows: 7c's rows pass on FreeBSD and arm64 in CI

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status platform-rows`.

## Outcome

Phase 7c's functional rows run in CI on FreeBSD and arm64, so C5 is a measurement, not a stated miss (D90). Codex task C3 (FreeBSD CI) feeds it.

## Acceptance criteria

- A1 [artifact] FreeBSD: a CI job builds and runs the suite and gate7c_rows.sh's functional rows: green runs linked in docs/dev/freebsd.md
- A2 [artifact] arm64: the same on GitHub's arm64 runners: green runs linked in docs/dev/platforms.md
- A3 [manual] Failures found are fixed or each is a named quest (the lead checks the rows against the hunt list)

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
