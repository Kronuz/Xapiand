# cli-restore-records: journal

State: working
Quest: .campaign/xapiand/quests/cli-restore-records.md (the lead's; never edit it)

## Doing

Proof job: build head, run `tests/restore_cli_check.sh` (must pass), then the mutant (restore resolves
read-only) and main's src (`git merge-base HEAD master`), each must fail. Plus `campaign verify`.

## Next

1. Record the proof job and the build-test check in Evidence.
2. CHANGELOG entry once the file set allows it (Blocked on).

## Blocked on

lead: the changelog. The file set names `CHANGELOG.md`, which does not exist; the changelog is
`docs/src/content/docs/changelog.md` (`CHANGELOG` at the root is a symlink to it), outside the file set.
Options: (a) add `docs/src/content/docs/changelog.md` to the file set and I add the entry (recommended; one
bullet under Unreleased "Fixed"); (b) the lead or docs-upgrade-page writes it. Text ready:
"- **`--restore` into a new, empty data directory writes the index's settings.** The CLI restore resolved
the index read-only, so `.xapiand/indexes` lacked its shard records and a node serving the directory answered
`500` ("Inconsistency in number of replicas") or counted 0 documents. It now resolves the index as `RESTORE`
does, from the dump's first object."

## Cause

The HTTP path: `restore_database_view` (src/server/search_views.cc) resolves the index with
`XapiandManager::resolve_index_endpoints(Endpoint(path), writable=true, &first_object)`, so
`IndexResolverLRU::resolve_index_settings` runs `save_settings` (src/index_resolver_lru.cc), which writes the
`INDEX` record (`number_of_shards`, `number_of_replicas`) and, through `save_shards`, each `INDEX/.__N` record
with its `shards` node list into `.xapiand/indexes`.

The CLI path: `restore_documents()` (src/main.cc) resolved with main.cc's `resolve_index_endpoints(endpoint)`,
which calls `XapiandManager::resolve_index_endpoints(endpoint)` with writable=false: the settings were
computed and cached, never saved. The restore's schema save (`save_shared`, src/database/schemas_lru.cc) then
wrote the `INDEX` document in `.xapiand/indexes` holding only `_schema` (the default foreign schema link
`.xapiand/indexes/INDEX`). A node serving the directory: `load_settings` finds `INDEX` without
`number_of_shards`, reads it as one shard, finds no `shards` list, and throws "Inconsistency in number of
replicas configured for '.../.xapiand/indexes'" (src/index_resolver_lru.cc:377). On disk in the reproduction's
directory A: one shard of `.xapiand/indexes` (`.__5`) holds a segment, the other four none. A pre-created
index (case F) already had its records, so it worked.

Fix: `DatabaseHandler::restore_documents(endpoint, fd)` resolves the index itself after reading the first
batch, with the call RESTORE makes (`resolve_index_endpoints(endpoint, true, &batch.front())`), then indexes
with the same `DocIndexer`. No second code path: `save_settings` writes the records for both. An empty dump
resolves nothing and creates nothing, as an empty RESTORE.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending |
| A2 | pending |
| A3 | pending |
| A4 | pending; changelog blocked |
| A5 | pending |

Examined: reproduction 261109-174137-c5db06 (docs-upgrade-page), its directory A on disk.

## Result

<pending>

## Rulings I made

- The check serves each restored directory with the same `--name` and `--shards` the restore used:
  `.xapiand/indexes` itself is sharded by the runtime `--shards` (`resolve_index_settings`, its `.xapiand/`
  branch), so a directory served with another shard count reads another registry layout on either path,
  HTTP or CLI (code reading; outside this defect). The reproduction's `--shards 1` case served with the
  default count, hence COUNT 0.
- The restore resolves the index from the first object, as RESTORE does, not by a writable resolve before
  reading: RESTORE takes the first object's `_settings`, and the CLI now does too.
- The refusal names "1.0.0-alpha.10 or earlier". `src/xapian_data_guard.h` (comment "alpha.9 and earlier",
  and `UPGRADE_DOC`'s `#upgrading-from-100-alpha9` anchor) is outside the file set: left for the lead and
  docs-upgrade-page (the anchor must match the page's heading).
