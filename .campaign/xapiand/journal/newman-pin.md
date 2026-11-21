# newman-pin: journal

State: waiting on jobs 261121-050004-fa263e (A1), 261121-050012-2e49bc (A2); A3 on CI (test_lua red is the lua-method-flake quest's)
Quest: .campaign/xapiand/quests/newman-pin.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Pin is newman 6.2.3 in harness/package.json (+ package-lock.json). harness/newman_env.sh (sourced by
engine_e2e.sh and run_all.sh) runs `npm ci --prefix harness` when needed, self-ignores node_modules, puts it first on PATH,
prints "newman X (pinned ...)". ci.yml: the two "Install newman" steps removed; setup-node caches npm on the lockfile.
TESTING.md: new "Docs E2E" section. A1 job runs harness/newman_pin_check.sh (4.5.0 stub first on PATH).

## Next

1. Submit A1 (build without -GNinja), record id; push; A3 = CI Functional E2E (docs); test_lua red is the lua-method-flake quest's.

## Blocked on

Nothing. The lead widened the file set; arm64.yml, freebsd.yml, e2e_capture.sh and coverage.sh now use the same pin
(e2e_capture.sh sources newman_env.sh; FreeBSD runs `bash harness/newman_env.sh` as root in the VM before the suite runs as nobody).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | job 261121-050004-fa263e |
| A2 | job 261121-050012-2e49bc (the quest's full grep); one place = harness/package.json |
| A3 | CI after push |

## Result

(pending)

## Rulings I made

1. Pin in harness/package.json (not npx): lockfile gives integrity hashes; npm ci installs locally, no global.
2. harness/node_modules self-ignores via a `*` .gitignore the install writes (.gitignore is outside the file set).
3. The major>=6 check in e2e_capture.sh is gone: the pin replaces it.
4. e2e_capture.sh is now the one place every caller (gate7c_rows, coverage, run_all) gets the pin; engine_e2e.sh and run_all.sh also source newman_env.sh to print the version / skip early (idempotent).
5. Job 436565 failed in the build step, not the check: -GNinja mismatches the shared .deps subbuild dirs (Unix Makefiles). A1 is rebuilt with the default generator, like the other Xapiand jobs.
