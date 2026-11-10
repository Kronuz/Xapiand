---
title: "Frequently Asked Questions"
---

---
### Why I don't see the responses when I create/update objects?
For performance reasons, newly created or edited objects are not returned in
the response: a `PUT` with an ID answers `204 No Content`, and a `POST` without
an ID answers `201 Created` with the new path in the `Location` header. If you
wish to get objects echoed in the response, turn on the `--echo` option, pass
`?echo` query param or increase verbosity to level 4 or higher (`-vvvv` or
`--verbosity=4`).

---
### Why the responses are unformatted JSON one-liners?
For performance reasons, to see pretty formatted responses use `--pretty`
option, pass `?pretty` query param or increase verbosity to level 4 or higher
(`-vvvv` or `--verbosity=4`).

---
### Where does Xapiand keep my data?
In the node's data directory: `<prefix>/var/db/xapiand` unless you give another
with `-D` (`--database`); the Docker image uses `/var/db/xapiand`. See
[Options](/Xapiand/options#node-and-data).

---
### Which port does it listen on?
8880, over HTTP, or over HTTPS when the node has a `--tls-cert` and a `--tls-key`.
`--port` sets another and `--bind-address` the address.

---
### Does the API need a password?
No. The API has no authentication, and HTTPS only protects the traffic. Keep the
port where only trusted clients can reach it.

---
### Can I run a cluster?
Not in this release: Xapiand runs as a single node. The cluster options are gone
(see [Removed options](/Xapiand/options#removed-options)).

---
### How do I stop the node?
Send it `SIGINT` (`Ctrl-C`) or `SIGTERM`. It finishes the requests in flight,
commits and closes its indexes and exits; see
[Stopping the node](/Xapiand/options#stopping-the-node).

---
### Why does the node refuse to start on my old data directory?
A directory of indexes from `1.0.0-alpha.10` or earlier holds Xapian files, which
this release does not read, and the node exits with status 65. Dump them with that
release and restore them here; see [Upgrading](/Xapiand/upgrade#upgrading-from-100-alpha9).

---
### How do I delete an index?
There is no request for it: `DELETE /index/` answers `501 Not Implemented`. You
can delete the documents of an index one by one.

---
### How large can a request be?
A request body is read up to 16 MiB, and a larger one answers `413 Payload Too
Large`. Two requests stream instead: a `RESTORE` of NDJSON or MessagePack sent
with a `Content-Length`, and the `PUT` of a stored blob (up to 4 GiB).

---
### Do I need to pay to get a license?
No, Xapiand is free and open source (MIT license).

---
### How will my donation be used?

The donations will be directly used to support the developers.

---
### How do I donate?

We accept donations via PayPal:

<a class="paypalme" href="https://www.paypal.me/Kronuz/25" target="_blank" rel="nofollow">Donate to Kronuz via PayPal</a>
