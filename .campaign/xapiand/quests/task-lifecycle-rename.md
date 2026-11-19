# task-lifecycle-rename: task docs: serve/service become lifecycle/component

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status task-lifecycle-rename`.

## Outcome

task's service-shape design uses D132's names: `task::lifecycle` drives `component`s and takes a stop source (signals are one source; a set of components is a component), matching cpp-libraries docs/server-composition.md at its CURRENT commit. No code in task uses the old names (task:95798a0).

## Acceptance criteria

- A1 [artifact] `docs/design/service-shape.md` names `lifecycle`, `component` and a stop source throughout (today :12-14, :43, :46, :94, :101-120, :126, :132, :156-168, :200, :207, :257), with nesting stated; signals survive only as one stop source
- A2 [artifact] `docs/API.md:135,147`'s example coroutine is renamed so `serve` names nothing in task's docs
- A3 The doc check passes and CI is green

## Contracts

Docs only; the adoption quests build the code under these names.

## Decisions

- D132 (owner 2026-11-19).

## May land early

None; it is one docs change.
