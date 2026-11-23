# agents-md: AGENTS.md says how Xapiand is built and worked on today

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status agents-md`.

## Outcome

Xapiand's AGENTS.md is the short, current entry point an agent reads first: what the repository is,
how it is built and tested (CMake and Ninja only, D168), the hard rules (GPL sources never read:
`src/xapian/**`, `src/sortable_serialise.*`, `contrib/python/xapiand-py/xapiand/utils/sortable.py`;
identity; no AI attribution), where docs live (`docs/`, `docs/dev/`, `.campaign/xapiand/`), and that
work runs as campaign xapiand (the campaign skill is authoritative for process). Pre-campaign
guidance that is stale (the PR-era "same PR" changelog rule, `cmake .. && make`, the release steps
the release quests replaced) goes; still-true detail moves to `docs/dev/` and is linked. Detent's
AGENTS.md is the model for shape and length. Owner, 2026-11-21: "Do we have bad or contradictory
instructions in our AGENTS.md file?"

## Acceptance criteria

- A1 [artifact] `AGENTS.md` at most 80 lines, with every rule above and no instruction that
  contradicts the campaign skill or D168 (the `lines` check counts lines)
- A2 [artifact] Every section removed from AGENTS.md is either stale (the journal names why, one line
  each) or moved to a `docs/dev/` page that AGENTS.md links

## Contracts

None (agent guidance only).

## Decisions

- Lead, 2026-11-21: rewrite to Detent's shape; the owner was told and did not object.

## May land early

None.
