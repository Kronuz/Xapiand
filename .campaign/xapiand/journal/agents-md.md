# agents-md: journal

State: ready for review
Quest: .campaign/xapiand/quests/agents-md.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Done.

## Next

1. Lead review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `AGENTS.md` is 55 lines; rules present: GPL paths, identity, no attribution, Ninja-only build, campaign skill authoritative, docs locations. |
| A2 | `docs/dev/agent-codebase.md` holds the moved sections; removed sections listed below, one line each. |

A2, removed sections:
- Build & toggles: `cmake .. && make` stale (D168); replaced by CI's Ninja commands; toggles are in docs/dev/testing.md.
- Verifying changes (docs-as-E2E): `docs_to_postman.py | newman` stale; CI runs `harness/run_all.sh e2e tls`, kept; detail in docs/dev/testing.md.
- First, the big split: folded into the intro; the Xapian-fork and de-vendor history dropped (history).
- Where things live (code map table): moved to docs/dev/agent-codebase.md, linked.
- Conventions you'll see everywhere (L_* macros, reactor, MsgPack, perfect hash, serialization order, banner font): moved to docs/dev/agent-codebase.md.
- Load-bearing invariants: moved to docs/dev/agent-codebase.md.
- Traps: moved to docs/dev/agent-codebase.md.
- Making changes well: moved to docs/dev/agent-codebase.md.
- Releasing & changelog ("same PR" rule, tag steps): stale, replaced by the release quests; only the changelog location is kept.

## Result

AGENTS.md rewritten to 55 lines in Detent's shape; added the Hard rules and machine-sharing sections. Moved code map, conventions, invariants, traps, change tips to docs/dev/agent-codebase.md. Ninja form written as `-GNinja` per CI.

## Rulings I made

- The copyright-line rule is kept in Hard rules (owner's standing rule).
- Release/changelog procedure dropped rather than moved: stale per the quest.
