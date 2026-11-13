# task-lanes: An opt-in heavy lane in task's budget and asio_scheduler

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status task-lanes`.

## Outcome

Kronuz/task gains the heavy lane of Xapiand's docs/design/loop-scheduling.md D5 (D79-D82): `budget(slice, token, light_slices)` counts the slices a request spends; at or past `light_slices` its scope turns heavy for life, unless `rearm_light(A)` gives a stream chunk an allowance. A heavy scope's resumptions go through `post_demoted` to a lane `asio_scheduler::drain()` runs one turn per io_context pass, so light work and timers run first. `light_slices` 0, the default, keeps today's behaviour exactly for Detent, prospect, completion and consensus. `scheduler_ref` stays 16 bytes, with the lane packed into the low bit of `vt_`.

## Acceptance criteria

Three to five, at outcome level: `- A1 The lane works as D5 states, with a test each that fails without it: a light scope turns heavy at `light_slices`; heavy resumptions run after light work and timers in the same pass; `rearm_light(A)` gives a chunk A slices; `operator==` ignores the lane (review F4); `light_slices` 0 never goes sticky-heavy, with a mutant (F6); a heavy `when_all` child that finishes last does not leave the next chunk heavy, or the note's rule is changed and stated (F12); `post_demoted` from a WorkPool thread hops to the loop: the suite (build-test), with `--fails-without` jobs
- A2 `post_demoted` and the lane bit are race-free across threads: the suite under TSan, one light job
- A3 With the lane compiled in but `light_slices` 0, a resumption costs nothing measurable against task main (Detent's condition, msg-40 reply): one exclusive paired A/B of the resume path of at most 10 minutes (D59), honest magnitude
- A4 [manual] Detent's suite passes against this branch's sha through Detent's integration job (Detent's condition): its lead names the job id
- A5 [artifact] A how-it-works note for the lane, the classing and the default: `docs/lanes.md`

## Contracts

Provides `task:lanes`: `budget(slice, token = {}, light_slices = 0)`, `budget::spent()`, `budget::rearm_light(allowance)`, `task::enter_light()`, `scheduler_ref::heavy()`, and `asio_scheduler::post_demoted` callable from any thread when the executor is an io_context's. No change for callers that pass no `light_slices`. Consumed by http-lane and Xapiand's loop-scheduling.

## Decisions

- Owner, 11-12 (D79-D82): the lane, K 8 for Xapiand, stream chunks light for 4 slices, today's loop count.
- Lead, 11-12: Detent's two conditions (A3, A4) come from its lead's reply to msg-40; send the sha when A1-A3 are green.
- Lead: the design and its review are in Xapiand's docs/design/loop-scheduling.md and .campaign/xapiand/reviews/loop-scheduling-design.md (APPROVE at d69ac733f); this quest builds it, it does not redesign it.

## May land early

Tests and docs.
