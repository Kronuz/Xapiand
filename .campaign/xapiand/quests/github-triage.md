# github-triage: Address and close the GitHub issues and PRs

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status github-triage`.

## Outcome

Every open issue and pull request on github.com/Kronuz/Xapiand has a verdict and a drafted reply, and
the ones this quest can settle are settled: the twelve Dependabot pull requests (#51, #54-#61, #64-#66)
are applied as one change to `docs/` and `docs/discussions/worker/` lockfiles, so GitHub closes them on
landing; each issue (#2, #11, #13, #14, #18, #21, #24-#29, #33-#35, #39) is answered from the code and
docs as they are now. The lead posts the replies and closes issues after landing; closing an issue as
"not planned" is the owner's call, asked once for the batch.

## Acceptance criteria

- A1 The Dependabot bumps are applied (each PR's version or newer, `npm audit` no worse): the `site` check (npm ci and the Astro build) and the `worker` check (its tests) pass at the landed code, and the journal lists each PR with the version it moved to
- A2 [artifact] `docs/dev/github-triage.md` has a row per open issue: number, title, verdict (answered: works now, with the page or code that shows it; fixed by a landed quest; becomes a hunt row or quest, with its text; not planned, with the reason, for the owner), and a drafted reply under 120 words in plain technical prose: `docs/dev/github-triage.md`: "## Issues"
- A3 [artifact] Every claim a drafted reply makes about current behavior cites `file:line` or a docs page, and features strata does not serve are answered as removed, never as coming: `docs/dev/github-triage.md`: "## Issues"
- A4 [artifact] The issues whose answer is new work are listed as proposed hunt rows (target, evidence, start in, repo) for the lead to file: `docs/dev/github-triage.md`: "## Proposed hunt rows"

## Contracts

None. No GitHub write: the contributor never comments, closes or merges; the lead does after landing.

## Decisions

- Owner, 11-09: go through all issues and PRs, addressing and closing them when done.

## May land early

Nothing; one landing.
