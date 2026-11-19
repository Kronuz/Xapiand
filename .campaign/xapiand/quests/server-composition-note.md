# server-composition-note: Fold the design review into the server-composition note and mark it CURRENT

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status server-composition-note`.

## Outcome

<The behavior that exists when this quest lands, in one paragraph.>

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 <criterion>: <evidence>

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>

## Outcome

`cpp-libraries/docs/server-composition.md` is the reviewed shape every Kronuz server adopts (D74): Xapiand, detentd, detent-relay and Prism. Its status line says CURRENT at a named commit, and every cited file:line holds at the heads it names.

## Acceptance criteria

- A1 [artifact] Findings F1-F6 of `.campaign/xapiand/reviews/server-composition-design.md` are folded in. F1: `--tls-ca` states what it does today (no client authentication). F2: the descriptor budget is its own check for plain and TLS alike; it raises the soft limit first, states its reserve, and covers an app's own descriptors (Xapiand's database pool). F3: one place per bound (`Limits` absorbs or names `AsioResponseOptions`; handshake bounds live in one place). F4: the process steps (daemon and readiness, pid lock, log session, privilege drop, data-directory lock). F5: a rule for how code reaches state services (no global accessor). F6: the 1 ms loops move to wheel waits unless a D62 measurement keeps them.
- A2 [artifact] The stale items are corrected: line citations at the named heads, the Mains row for Xapiand's shutdown retry, the sizing (8.6 KB plain / 40 KB TLS per idle connection), the relay's mirror service, and C2.
- A3 [artifact] Each deviation between the three apps is listed with its reason, or is removed.
- A4 [artifact] The status line reads CURRENT at the commit that lands, naming the app and library heads the citations hold at.
- A5 [manual] Detent's lead confirms the Detent-side text (F1, F4, F6) before landing.

## Notes

Documentation only: no code changes. The Xapiand adoption quest (manager.cc and main.cc onto `serve`) and Detent's detentd/relay adoption are filed after this lands, against its commit.
