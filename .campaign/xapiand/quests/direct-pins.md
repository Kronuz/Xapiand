# direct-pins: Xapiand declares only the libraries it includes

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status direct-pins`.

## Outcome

Xapiand's `CMakeLists.txt` declares only the Kronuz libraries Xapiand includes directly, each at the
head every library pins, and the early re-declarations that override stale transitive pins are
gone: no override is kept (owner, 2026-11-02, D18).

## Acceptance criteria

- A1 Every `FetchContent_Declare` in `CMakeLists.txt` names a library a Xapiand source includes directly (carried "After the in-family pin wave"): a script listing each declaration and its include site, in the `build-test` check
- A2 No declaration exists only to win over a transitive pin; the "declared first, so it wins" re-declarations are deleted (carried, D18): `git grep` in the `build-test` check
- A3 Each pin equals the commit every library that also pins it uses (carried, D18): the script of A1 compares the resolved `_deps` trees
- A4 Release, unit tests and docs E2E unchanged: the `build-test` and `e2e` checks

## Contracts

The build's resolved library commits; any library whose pins disagree is fixed in that library
first (a separate quest per repository, ordered with `--after`).

## Decisions

- Open (lead): master still carries "declared first" blocks (for example compressors and hashes in
  `CMakeLists.txt`); a library whose pins disagree needs its own repin before A2 can hold.

## May land early

None.
