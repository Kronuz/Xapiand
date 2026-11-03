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
- `--name` sets the node name.
- `--shards` sets the default number of shards per index (5).
- `--port` sets the HTTP REST API port.
- `-d`, `--detach` runs the process in the background.
- `--processors` sets how many processors the node uses (all by default). It runs one HTTP loop per processor.

## Write-Ahead Log

- `--writers` and `--wal-writer-cache-size` tune the
  [WAL](/Xapiand/reference-guide/wal) writer pool.

## Stopping the node

`SIGINT` or `SIGTERM` stops the node. The stop has two waits, and the node exits when the second ends:

1. **The clients.** The node waits for the requests in flight. With clients that keep sending, this has taken up to about 45 s.
2. **The rest, at most 35 s in all.** In this order, each step starting when the one before it has ended:
   1. The HTTP service stops: it stops accepting, a read ends at its next checkpoint, an admitted write is let finish (one that waits for its index's turn answers `503` after 30 s), and the loops stop.
   2. The autocommits that were waiting are dropped; closing an index commits what it has pending.
   3. Every index closes.
   4. The write-ahead logs flush.
   5. The fsync threads finish.

What is still running when the 35 s end (steps 1 to 3 share them) is logged and abandoned: the requests in flight per loop, and the indexes whose turn or references are held. The node then flushes the
write-ahead logs and exits. Without abandoned work the status is 0; with it the status is 75 (`EX_TEMPFAIL`) and the log lists what was abandoned. An index that cannot close within the 35 s (for example on a hung
disk) counts, whether or not a request is in flight: the node logs `Shutdown abandoned the indexes that are still in use` and exits 75. The next start replays the log. A write answered with a 2xx
is in the log; one that was not answered was never promised.

A second `SIGINT` or `SIGTERM` during the second wait ends it at once: the node logs what is unfinished, flushes the logs and exits 75. The longest stop is therefore the first wait plus 35 s, plus the
flush of the logs; set a supervisor's stop timeout above that (for example 120 s). A node that cannot set itself up (a port that is taken) logs why and exits with status 70.

## HTTPS

- `--tls-cert` and `--tls-key` (PEM files, given together) serve the REST API over TLS on
  `--port`, in place of plain HTTP. The key file must be readable by its owner only (mode
  `0600`); the certificate must be valid now, and Xapiand warns when it expires within 14
  days. TLS 1.2 and 1.3 only, forward-secret ciphers only, no session resumption.
- `--tls-max-connections` caps the TLS connections held at once (1024 by default; it must
  fit the open-files limit). Over TLS, a connection idle 75 seconds, headers not done in 30
  seconds, or a request or reply that moves no data for 60 seconds is closed.

HTTPS protects traffic between clients and the node. It does not authenticate clients:
anyone who can reach the port can use the API.

For the complete, authoritative list, run `xapiand --help`.
