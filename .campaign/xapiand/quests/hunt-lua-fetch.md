# hunt-lua-fetch: Lua sources fetch without lua.org resets

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status hunt-lua-fetch`.

## Outcome

Configuring Xapiand fetches the pinned Lua tarball from an ordered list of URLs (lua.org first, then a mirror), each checked against the same SHA256, so a connection reset on one host no longer fails a job or CI run. A local tarball given by `XAPIAND_LUA_URL` (or a file path) still wins. Hunt row: lua.org fetch resets (11-13), 4+ failed jobs (VM 034617, mac 050321-24afc5/c8007c).

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 With lua.org unreachable (its URL pointed at a closed local port), configure still fetches Lua from the next URL and the hash matches: a job that fails without the fallback.
- A2 A tarball whose SHA256 differs from the pin is refused on every URL: a job.
- A3 The build check passes, and CI is green on every platform.

## Contracts

None: the build's own fetch.

## Decisions

The mirror must be a public, stable URL serving the identical tarball (GitHub release of lua/lua or a Kronuz release asset); no new paid service.

## May land early

Everything; it is low risk.
