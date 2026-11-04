#!/usr/bin/env python3
"""qw_crash_probe.py -- what a Quickwit SIGKILL leaves, step by step (D5's process-crash window for Q).

Starts Quickwit, publishes 5,000 documents (commit=force), appends batches with commit=auto for 12 s, SIGKILLs,
restarts on the same data, and prints the searchable count and the shard and split listings every few seconds for 120 s.

  qw_crash_probe.py QUICKWIT_BINARY WORKDIR [PORT]
"""

from __future__ import annotations

import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import systems as sy


def tree(path: str) -> str:
    out = []
    for root, _dirs, files in os.walk(path):
        size = sum(os.path.getsize(os.path.join(root, f)) for f in files if os.path.exists(os.path.join(root, f)))
        out.append(f"{os.path.relpath(root, path)}: {len(files)} files, {size} bytes")
    return "\n".join(out[:25])


def main() -> int:
    binary, workdir = sys.argv[1], sys.argv[2]
    port = int(sys.argv[3]) if len(sys.argv) > 3 else 17270
    system = sy.make("quickwit", f"127.0.0.1:{port}")
    schema = sy.Schema.load("logs")
    system.start(sy.Launch(binary, workdir, port, cpus=2))
    client = sy.Client(system.target)
    system.setup(client, "p", schema)
    docs = [
        json.dumps(
            {
                "@timestamp": 893964617 + n,
                "clientip": "1.2.3.4",
                "request": f"GET /a{n} HTTP/1.0",
                "status": 200,
                "size": n,
                "stream": "x",
                "seq": n,
            }
        ).encode()
        for n in range(5000)
    ]
    status, body = client.call(system.bulk_request("p", schema, docs, last=True))
    print("published", status, body[:100], flush=True)
    sent = acked = 0
    t0 = time.time()
    while time.time() - t0 < 12:
        batch = [
            json.dumps(
                {
                    "@timestamp": 893970000 + sent + k,
                    "request": "GET /b",
                    "status": 200,
                    "size": 1,
                    "stream": "w",
                    "seq": sent + k,
                }
            ).encode()
            for k in range(100)
        ]
        status, body = client.call(system.bulk_request("p", schema, batch))
        if status < 300:
            acked += 100
        sent += 100
        time.sleep(0.1)
    print(f"appended {sent}, acked {acked}; before the kill:\n{tree(system.datadir)}", flush=True)
    count_q = sy.Query("all", command="count")
    print("count before kill", system.run(client, "p", schema, count_q).count, flush=True)
    system.stop(kill=True)
    t_kill = time.time()
    print("killed; after:\n" + tree(system.datadir), flush=True)
    system.restart(kill=True)
    client = sy.Client(system.target)
    print("restarted", flush=True)
    last = None
    while time.time() - t_kill < 140:
        ans = system.run(client, "p", schema, count_q)
        st, sp = client.call(sy.Request("GET", "/api/v1/indexes/p/splits?split_states=Published"))
        cl = client.call(sy.Request("GET", "/api/v1/cluster"))[1][:200]
        row = (ans.count, len(json.loads(sp).get("splits", [])) if st == 200 else None)
        if row != last:
            print(f"+{time.time() - t_kill:5.1f}s count={row[0]} published splits={row[1]} cluster={cl!r}", flush=True)
            last = row
        time.sleep(2)
    print("after:\n" + tree(system.datadir))
    system.stop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
