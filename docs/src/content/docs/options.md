---
title: "Options"
---

An updated list of all available options for Xapiand can be retrieved using
`xapiand --help`.

## Verbosity

Verbosity of the servers logs can be set by using the `-v`, `--verbose` or
`--verbosity` options. _*very-very-very* verbose_ output is usually enabled
with `-vvvv` or `--verbosity=4`. This mode also enables `--human`, `--echo`,
`--pretty` and `--comments` options by default.

### Echo

Echo makes Xapiand return newly created or edited objects as part of the
response. Usually when creating a new object Xapiand will return `201 Created`
HTTP response code, without a body and `204 No Content` HTTP response when
updating existing objects, also without a body. Returning a body can be enabled
with the `--echo` option or by using a verbosity level higher or equal to 4.

### Pretty

Pretty makes Xapiand return pretty (formatted) JSON output as responses. This
option can be enabled with the `--pretty` option or by using a verbosity level
higher or equal to 4.

### Human

Human makes Xapiand return humanized numbers for size and times in the output
of the responses. This option can be enabled with the `--human` option, by
setting output to be "pretty" or by using a verbosity level higher or equal to 4.

### Strict

Schemas are normally automatically created by default, guessing the type of new
fields being indexed. The `--strict` option disables this
[Dynamic Typing](/Xapiand/reference-guide/schemas/dynamic-typing)
feature and forces the user to specify a type for all new fields, making the
request fail with `412 Precondition Failed` if the datatype is missing.

## Node and Data

- `-D`, `--database` sets the path to the root of the node, where all index data
  lives.
- `--name` sets the node name; `--cluster` sets the cluster it joins.
- `--port` sets the HTTP REST API port.
- `-d`, `--detach` runs the process in the background.
- `--solo` runs a single-node indexer with no replication or discovery, useful for
  local development and tests.

## Index Defaults

New indexes inherit these unless overridden per index (see
[Data Replication](/Xapiand/reference-guide/indexes/replication)):

- `--shards` sets the default number of primary shards per index.
- `--replicas` sets the default number of replicas per index.

## Cluster and Networking

- `--discovery-port`, `--discovery-group` and `--discovery-interface` control the
  UDP multicast used to discover other nodes. Pin `--discovery-interface` (e.g. to
  `127.0.0.1`) on a multi-homed or VPN host where the default interface can't
  multicast.
- `--replica-port` is the TCP port for the replication protocol.
- `--primary-node` names the node holding the primary cluster database, and
  `--database-stall-time` is how long to wait before a shard may be promoted to
  primary.
- `--writers` and `--wal-writer-cache-size` tune the
  [WAL](/Xapiand/reference-guide/wal) writer pool.

For the complete, authoritative list, run `xapiand --help`.
