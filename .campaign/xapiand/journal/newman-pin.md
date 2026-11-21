# newman-pin: journal

State: waiting on jobs 261121-032949-19a6ee (A1), 261121-032951-b7522b (A2); A3 on CI after push
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

1. Record A1/A2 job ids; push; check CI Functional E2E (docs) for A3.

## Blocked on

Not blocking: .github/workflows/arm64.yml (line 60) and freebsd.yml (line 63) still `npm install -g newman@6`; outside my
file set. The lead should widen it (replace with setup-node/npm and the harness pin) or accept. A2 is run over
harness, ci.yml, TESTING.md.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | job 261121-032949-19a6ee |
| A2 | job 261121-032951-b7522b (scoped, excludes e2e_capture.sh); one place = harness/package.json |
| A3 | CI after push |

## Result

(pending)

## Rulings I made

1. Pin in harness/package.json (not npx): lockfile gives integrity hashes; npm ci installs locally, no global.
2. harness/node_modules self-ignores via a `*` .gitignore the install writes (.gitignore is outside the file set).
3. e2e_capture.sh (outside the file set) is untouched: it finds the pinned newman via the PATH its callers set; its major>=6 check stays.
4. harness/coverage.sh (outside file set) still gates its e2e on `command -v newman`; a lead follow-up.
