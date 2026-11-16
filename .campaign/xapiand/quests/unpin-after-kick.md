# unpin-after-kick: A catalog unpin reads nothing after it kicks the slot's close

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status unpin-after-kick`.

## Outcome

`catalog::unpin` touches no catalog state after `kick(idx)`: strata:5d3c948a made unpin end `kick(idx); over_soft();`, and at shutdown the kick posts the slot's close, which can let the owner destroy the catalog (Xapiand's `~DatabasePool`, pool.cc:655) before `over_soft()` reads it. TSan reported it in Xapiand's test_node on the strata bump (CI run on Xapiand:05cba6694, catalog.hh:1430). The soft-limit check runs before the kick, or the kick is the last access, with the soft-limit behaviour unchanged.

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 A test in which the owner destroys the catalog as soon as a slot's close is posted, while another thread's unpin runs, is clean under TSan/ASan at the fix and fails without it: a job under a sanitizer build, plus a `--fails-without` job
- A2 The soft-limit relief still triggers on the unpin that crosses the soft limit (an existing or new test): the suite
- A3 CI green on every platform, Linux TSan included: the build-test job and CI

## Contracts

No API change. Xapiand's xapiand-strata-bump re-pins strata at this fix.

## Decisions

- Lead, 11-12: found by xapiand-strata-bump's CI; fixed in strata at the root, not worked around in Xapiand.

## May land early

None.
