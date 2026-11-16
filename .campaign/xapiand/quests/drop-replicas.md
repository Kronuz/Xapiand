# drop-replicas: Remove the replica settings left from the cluster

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status drop-replicas`.

## Outcome

The replica settings the removed cluster left behind are gone (remove-not-refuse, D7): a strict new
index no longer requires `number_of_replicas` (`src/index_resolver_lru.cc:586`), a request that sends
it is accepted and the key ignored, as other unknown `_settings` keys already are, and GET `_settings`
no longer shows it, for new and old indexes alike. `num_replicas_plus_master`, `settle_replicas` and
`opts.num_replicas` are deleted; the shard-record guard that remains ("a shard record names no
node") keeps its check under a name that says so. `number_of_shards` and the `.__N` records stay.
Explorer report (code reading, 11-09): every replica use is vestigial except that guard.

## Acceptance criteria

- A1 A strict `PUT /idx/` without `number_of_replicas` answers 2xx; with `number_of_replicas: 2` answers 2xx and GET `/idx/_settings` has no `number_of_replicas`: `harness/replicas_check.sh` as a job, failing at master's code for the first case
- A2 A data directory written by master's binary (with `number_of_replicas` in its records) still opens, COUNTs and DUMPs the same documents under the new binary: in A1's check
- A3 `git grep -nE 'num_replicas|settle_replicas|plus_master'` in `src/` is empty, and the guard's error message names the missing node instead of replicas: a job running the grep, and the `build-test` check
- A4 `harness/terms_cap_check.py` no longer sends the key; ARCHITECTURE.md no longer describes replicas: [artifact] quotes in the journal
- A5 The `build-test` check passes, and CI is green; the docs pages (create-api.md, namespace.md) are docs-site-refresh's, listed in the journal for it

## Contracts

HTTP: `number_of_replicas` in `_settings` becomes an ignored key; GET `_settings` drops it.

## Decisions

- Lead, 11-09: accept and ignore, not 400 (a refusal would break remove-not-refuse and Elasticsearch-style clients). The per-shard node list is vestigial too, but changing the record layout is a separate decision.

## May land early

Nothing; one landing.
