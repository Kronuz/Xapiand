# github-triage: journal

State: ready for review
Quest: .campaign/xapiand/quests/github-triage.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Done: the five dependency ranges are bumped (one commit, Xapiand:71acc13ea) and `docs/dev/github-triage.md` is written (Xapiand:0aff10e6d). Worker check 261109-184439-2432a6 and npm audit 261109-184445-d9cf89 passed at the head; waiting on the site build 261109-184442-cdf29d; on green, set State to "ready for review".

## Next

1. Review.

## Blocked on

Nothing. For the owner (asked once, in `docs/dev/github-triage.md` "## Owner calls"): "not planned" for #24, #26, #34, the at-rest half of #25, and #28 unless retested; wording of #33 (license) and whether #27's reply may state that a smoke run was behind Elasticsearch.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | worker check 261109-184439-2432a6 and site build 261109-184442-cdf29d at Xapiand:0aff10e6d; `npm audit` 0 vulnerabilities in `docs/` and the worker, at master 261109-183455-d2d240 and before the docs note at 261109-183445-d536dd, and at the head 261109-184445-d9cf89. Earlier green at Xapiand:71acc13ea: worker 261109-183447-d508bf, site 261109-183515-72a7f3 |
| A2 | `docs/dev/github-triage.md`: "## Issues" |
| A3 | `docs/dev/github-triage.md`: "## Issues" |
| A4 | `docs/dev/github-triage.md`: "## Proposed hunt rows" (and "## For Detent" for the owner's Detent addition) |

### A1: each PR and the version it moved to

The lockfiles on master already resolved every bumped package at or above the PR's version; the commit raises the ranges still declared lower. Master lockfile resolves, in brackets.

| PR | Package | Moved to |
|---|---|---|
| #51 | postcss 8.5.25 | already [8.5.30] |
| #54 | nanoid 3.3.18 | already [3.3.20] |
| #55 | svgo 4.1.0 | already [4.1.0] |
| #56 | hono 4.13.5 | worker range `^4.13.5` [4.13.13] |
| #57 | astro 7.3.2 | docs range `^7.3.2` [7.4.0] |
| #58 | sharp 0.35.4 | already `^0.35.5` [0.35.5] |
| #59 | sharp 0.35.4, wrangler 4.131.0 | already [sharp 0.35.5, wrangler 4.157.0]; wrangler range from #64 |
| #60 | smol-toml 1.8.0 | already [1.9.1] |
| #61 | js-yaml 4.3.2 | docs range `^4.3.2` [4.3.2] |
| #64 | undici 7.29.0, wrangler 4.145.0 | worker wrangler range `^4.145.0` [wrangler 4.157.0, undici 7.30.1] |
| #65 | devalue 5.9.4 | already [6.0.3] |
| #66 | source-map-js 1.2.2 | already [1.2.2] |

The ranges changed in `docs/package.json`, `docs/package-lock.json` (its root entry), and the worker's two files, by hand: npm ran only in the jobs, whose `npm ci` fails if a lockfile and its `package.json` disagree.

## Result

`docs/dev/github-triage.md`: 16 issues, each with a verdict, a reply under 120 words (word counts 36 to 91), the `file:line` or page behind each claim, and a Detent column; six proposed hunt rows (H1 `_ngram`, H2 time limit, H3 highlighting, H4 index delete, already `hunt.md:28`, H5 accent folding, H6 `_store` docs); the owner's batch; nine notes for Detent. Verdicts: 5 fixed or works now (#2, #11, #13, #29, #39), 3 removed (#24, #26, #34), 3 answered (#25, #27, #33), 4 new work (#14, #18, #21, #35), 1 cannot verify (#28).

Not run: any node, any Cloud Run or gVisor container, a Linux build on the reporter's toolchain.

## Rulings I made

1. The Dependabot bumps were applied as range edits, not by running `npm update`: master's lockfiles already resolve every package at or above the PR versions, so a reinstall would have changed nothing but the ranges, and a `npm update` would have moved unrelated packages. The ranges follow the highest PR (wrangler `^4.145.0` from #64, not #59's 4.131.0).
2. The cluster issues (#24, #26) are answered as removed with no promise, although PLAN.md D9 parks cluster work after the release: the quest says never as coming.
3. #35 is not filed as a new hunt row: `hunt.md:28` already holds it.
4. #28 gets a retest request rather than a verdict: the SO_LINGER reading is an inference from the syscall's numbers.
5. Replies carry claims about the plan (the final Xapian-only release in #34, the first MIT-clean release in #33) only where PLAN.md decides them, and the note flags each for the owner.
