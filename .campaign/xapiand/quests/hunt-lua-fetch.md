# hunt-lua-fetch: Lua sources fetch without lua.org resets

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status hunt-lua-fetch`.

## Outcome

Xapiand fetches Lua like its other dependencies: from git, the Lua team's official https://github.com/lua/lua at v5.4.7, pinned by commit 1ab3208a1fceb12fca8f24ba57d6e13c5bff15e3, not a tarball from lua.org or a mirror (owner, 11-17). The limits patch applies to the repository's flat layout, and the drift check pins the commit, so a changed Lua still fails the build's tests.

## Acceptance criteria

- A1 Configure fetches Lua by commit from GitHub and builds, with no lua.org or mirror URL left: a job.
- A2 The drift check fails on a wrong commit or a changed patch: a job that fails with a wrong pin.
- A3 The tag's sources match the 5.4.7 release's src/ byte for byte, or each difference is named: a job.
- A4 The build check passes, and CI is green on every platform.

## Contracts

None: the build's own fetch.

## Decisions

Owner 11-17: fetch from github.com/lua/lua by commit, the same mechanism as the other dependencies; the tarball URL list goes.

## May land early

Everything; it is low risk.
