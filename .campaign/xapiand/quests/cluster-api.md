# cluster-api: Settings, health, :info, docs, changelog

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine
state (status, branch, checks, reviews) is in `state.json`: `campaign status cluster-api`.

## Outcome

Users see and control the cluster: `number_of_replicas` is back in `_settings` (set at create,
changed live through placement-repair's steps), `GET /:cluster/health` reports green, yellow or red
by replica count, `:info` lists each shard's group, members and leader, and `POST
/:nodes/<n>/:drain` drains a node. The docs describe the cluster (options, installation,
architecture, FAQ, reference guide, tutorials), the levels and their limits (design section 10),
and the changelog's `[Unreleased]` names every user-visible change against master.

## Acceptance criteria

- A1 Every route and setting above documented, and the docs E2E green (the `e2e` check)
- A2 `GET _settings` shows `number_of_replicas`; health is green with every shard at `rf`, yellow
  with one short, red with a group without quorum, each driven on 3 processes (a harness job)
- A3 `PUT _settings {number_of_replicas}` 0 to 2 and back on a live index returns at each step and
  health follows (a harness job)
- A4 The defaults: `number_of_replicas` and the shard count per cluster-release Q3 and Q9 (a test
  of a fresh index's settings)
- A5 [decision] cluster-release Q3 and Q9, pending owner; lead defaults (a) `number_of_replicas` 1
  as `rf = 2 + witness`, and (b) one shard by default
- A6 [artifact] `docs/src/content/docs/changelog.md`: `[Unreleased]` names the cluster, the levels,
  the removed multicast discovery and options, and the read-your-writes ruling (Q-F27)

## Contracts

Provides `xapiand:cluster-routes`: `number_of_replicas`, `GET /:cluster/health`, `:info`'s shard
section, and the drain route (routes per Q8). Consumes `xapiand:shard-map` and
`strata:placement-repair`.

## Decisions

- Design section 5: the changelog is `sortable-encoding`'s until it lands, so this quest needs it;
  `search_views.cc` is also distributed-search's, so this lands after it.
- Lead: the docs pages are this quest's; `upgrade.md` is cluster-migration's alone.

## May land early

The docs pages once A2's behavior has landed in shard-groups; the changelog last.
