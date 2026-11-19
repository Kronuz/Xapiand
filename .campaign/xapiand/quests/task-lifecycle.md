# task-lifecycle: task::lifecycle: components, stop sources, periodic and debounce

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status task-lifecycle`.

## Outcome

The task library provides the server composition's lifecycle (cpp-libraries docs/server-composition.md at cpp-libraries:0579a1a; task docs/design/service-shape.md): `task::lifecycle` runs `component`s (start(stop) task, stop(), drain(bound) task returning what it left undone, flush()), takes a stop source, and nests (a set of components is a component), with `periodic` and `debounce` on the loop's wheel replacing fixed-interval polling. Five daemons adopt it: Xapiand first, then detent-authority, detent-relay, detent-agent and Prism (D132, D138, Detent D122).

## Acceptance criteria

- A1 Stop order as the note states: every stop() in reverse start order, then each drain in reverse; a drain past its bound is cancelled at bound minus grace and abandoned at grace after; tests check the order and the returned leftovers, with a fake clock
- A2 Stop sources: a signal source (first signal stops, second hurries, third flushes then dies), a programmatic source (tests, admin QUIT), a parent component and a tool's end; tests for each, the signal one in a child process
- A3 Nesting: a lifecycle of components is a component, and a nested set stops inside its parent's order; a test with two levels
- A4 `periodic` and `debounce` run on the loop's wheel: no thread and no fixed-interval poll; tests check firing times with the fake clock and that stop cancels pending fires
- A5 A start failure stops the components already started, in reverse, and reports the failing one; a test
- A6 The suite and ThreadSanitizer runs pass, and CI is green on every platform
- A7 [artifact] docs/design/service-shape.md uses D132's names (lifecycle, component, stop source; `serve`, `service` and `stop_signals` as types are gone), and the README lists the API

## Contracts

Provides `task:lifecycle` as the note's step 5 and housekeeping section describe; the adoption quests (Xapiand's xapiand-lifecycle, Detent's server-composition-adoption, prism-server-composition) consume it. A shape change from the note goes to the lead before it is built.

## Decisions

- D132, D138 (owner 2026-11-19); risk high by consequence: a stop-order defect loses flushes in all five daemons and is invisible to their own tests.

## May land early

None: it is a shared public API, landed once after review.
