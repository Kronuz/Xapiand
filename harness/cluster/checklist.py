#!/usr/bin/env python3
# Copyright © 2026 Germán Méndez Bravo (Kronuz)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""The cluster checklist: the six items of the Xapian-era `cluster_check.sh`, and the history check.

1. membership: every node sees every member, and all agree on one leader.
2. read: a write on node 1 is read on every node.
3. search: a search from the last node finds it.
4. aggregations: `_doc_count`, `_sum` and `_avg` over 40 documents are exact on every node.
5. failover: a leader kill elects a new leader, writes resume, and the killed node rejoins.
6. replay: `kill -9` after 40 uncommitted writes, then a restart; every node serves all 40.
7. history: every write and get-by-id above, recorded, passes faultlab/history.py at its own levels.

Exit status: 0 when every item passes (or exactly the --expect-fail items fail), 1 when items fail,
2 when the harness itself fails (an item raised, a node never answered, the binary is missing, or
the run timed out).

harness/cluster/run.sh check [--nodes N] [--bin PATH] [--out DIR] [--expect-fail ITEM,...]
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import shutil
import signal
import sys
import time
import traceback
from collections.abc import Awaitable, Callable
from pathlib import Path

sys.path.insert(0, os.environ["FAULTLAB_PYTHON"])  # the pinned faultlab checkout (run.sh)

from faultlab import history
from faultlab.history import Recorder, Value

from cluster import Cluster, Node, Response, View, request

ITEMS = ("membership", "read", "search", "aggregations", "failover", "replay", "history")
AGG_DOCS = 40
REPLAY_DOCS = 40


class Item:
    def __init__(self, name: str) -> None:
        self.name = name
        self.failures = 0
        self.skipped = ""
        self.error = ""

    def check(self, ok: bool, text: str) -> bool:
        print(f"  {'ok:  ' if ok else 'FAIL:'} {text}", flush=True)
        self.failures += not ok
        return ok

    def note(self, text: str) -> None:
        print(f"       {text}", flush=True)

    @property
    def status(self) -> str:
        if self.error:
            return "error"
        if self.skipped:
            return "skip"
        return "fail" if self.failures else "pass"


async def until[T](probe: Callable[[], Awaitable[T | None]], seconds: float, pause: float = 0.5) -> T | None:
    """Calls `probe` until it returns something other than None, for at most `seconds`."""
    deadline = time.monotonic() + seconds
    while True:
        result = await probe()
        if result is not None or time.monotonic() >= deadline:
            return result
        await asyncio.sleep(pause)


class Checklist:
    def __init__(self, cluster: Cluster, recorder: Recorder, args: argparse.Namespace) -> None:
        self.cluster = cluster
        self.recorder = recorder
        self.wait: float = args.wait
        self.read_level: str = args.read_level
        self.read_query: str = args.read_query
        self.write_level: str = args.write_level
        self.tokens = 0
        self.probe_token: Value = None

    # --- recorded operations -------------------------------------------------------------------

    async def put(self, client: str, node: Node, index: str, doc_id: str, doc: dict[str, object]) -> Response:
        """PUTs a document carrying a fresh token, the value its history records."""
        self.tokens += 1
        token = f"t{self.tokens}"
        key = f"{index}/{doc_id}"
        self.recorder.record(
            client, "write", key, token, self.write_level, "invoke", node=node.name, launch=node.launches
        )
        response = await request(node.http, "PUT", f"/{index}/{doc_id}", doc | {"token": token})
        status = response.status
        outcome = "ok" if 200 <= status < 300 else "info" if status == 0 or status >= 500 else "fail"
        self.recorder.record(
            client, "write", key, token, self.write_level, outcome, node=node.name, launch=node.launches
        )
        return Response(status, token if outcome == "ok" else None, response.error)

    async def get(self, client: str, node: Node, index: str, doc_id: str) -> tuple[Value, Response]:
        key = f"{index}/{doc_id}"
        self.recorder.record(client, "get", key, None, self.read_level, "invoke", node=node.name, launch=node.launches)
        response = await request(node.http, "GET", f"/{index}/{doc_id}{self.read_query}")
        value: Value = None
        outcome = "info"
        if response.status == 200 and isinstance(response.body, dict):
            token = response.body.get("token")
            value = token if isinstance(token, str) else None
            outcome = "ok"
        elif response.status == 404:
            outcome = "ok"
        self.recorder.record(client, "get", key, value, self.read_level, outcome, node=node.name, launch=node.launches)
        return value, response

    async def read_until(self, node: Node, index: str, doc_id: str, token: Value, seconds: float) -> Response | None:
        """Polls get-by-id on `node` until it returns `token`; the last response when it never does."""
        last: list[Response] = []

        async def probe() -> Response | None:
            value, response = await self.get(f"get@{node.name}", node, index, doc_id)
            last[:] = [response]
            return response if value == token else None

        found = await until(probe, seconds)
        if found is None and last:
            body = json.dumps(last[0].body)[:160] if last[0].body is not None else last[0].error
            print(f"       {node.name} last read: http {last[0].status} {body}", flush=True)
        return found

    async def query(self, node: Node, index: str, query: dict[str, object]) -> dict[str, object] | None:
        response = await request(node.http, "POST", f"/{index}/:search?volatile", query)
        return response.body if response.status == 200 and isinstance(response.body, dict) else None

    # --- cluster views ---------------------------------------------------------------------------

    def live(self) -> list[Node]:
        return [node for node in self.cluster.nodes.values() if node.alive]

    async def views(self, nodes: list[Node]) -> dict[str, View | None]:
        found = await asyncio.gather(*(self.cluster.view(node) for node in nodes))
        return {node.name: view for node, view in zip(nodes, found, strict=True)}

    @staticmethod
    def agreed(views: dict[str, View | None]) -> str | None:
        leaders = {view.leader if view else None for view in views.values()}
        if len(leaders) == 1:
            return next(iter(leaders))
        return None

    async def converge(self, nodes: list[Node], members: frozenset[str], seconds: float) -> dict[str, View | None]:
        """Waits until each of `nodes` sees `members` and they agree on one leader among them."""
        last: dict[str, View | None] = {}

        async def probe() -> dict[str, View | None] | None:
            last.update(await self.views(nodes))
            complete = all(view is not None and members <= view.members for view in last.values())
            leader = self.agreed(last)
            return dict(last) if complete and leader is not None and leader in members else None

        await until(probe, seconds, 1.0)
        return last

    # --- the items -------------------------------------------------------------------------------

    async def membership(self, item: Item) -> None:
        nodes = list(self.cluster.nodes.values())
        members = frozenset(node.name for node in nodes)
        views = await self.converge(nodes, members, self.wait)
        for name, view in views.items():
            seen: list[str] = sorted(view.members) if view else []
            item.check(view is not None and members <= view.members, f"{name} sees {seen} ({len(seen)}/{len(nodes)})")
        leader = self.agreed(views)
        leaders = sorted({str(view.leader) for view in views.values() if view})
        item.check(leader is not None and leader in members, f"one leader leads (the nodes name {leaders})")

    async def read(self, item: Item) -> None:
        first = self.cluster.nodes[1]
        put = await self.put("put@node1", first, "cluster_probe", "1", {"title": "hello cluster", "n": 42})
        item.check(put.body is not None, f"PUT cluster_probe/1 on node1 (http {put.status})")
        self.probe_token = put.body
        if put.body is None:
            return
        nodes = list(self.cluster.nodes.values())
        found = await asyncio.gather(
            *(self.read_until(node, "cluster_probe", "1", put.body, self.wait) for node in nodes)
        )
        for node, response in zip(nodes, found, strict=True):
            item.check(response is not None, f"GET cluster_probe/1 on {node.name} returns the write")

    async def search(self, item: Item) -> None:
        last = self.cluster.nodes[len(self.cluster)]
        query: dict[str, object] = {"_query": {"title": "hello"}}

        async def probe() -> dict[str, object] | None:
            body = await self.query(last, "cluster_probe", query)
            hits = body.get("hits") if body else None
            found = isinstance(hits, list) and any(isinstance(h, dict) and h.get("n") == 42 for h in hits)
            return body if found else None

        found = await until(probe, self.wait)
        item.check(found is not None, f"SEARCH from {last.name} finds cluster_probe/1")

    async def aggregations(self, item: Item) -> None:
        first = self.cluster.nodes[1]
        total = 0
        for k in range(1, AGG_DOCS + 1):
            age = k % 30 + 1
            total += age
            put = await self.put("put@node1", first, "cluster_agg", str(k), {"age": age})
            if put.body is None:
                item.check(False, f"PUT cluster_agg/{k} on node1 (http {put.status} {put.error})")
                return
        aggs = {"s": {"_sum": {"_field": "age"}}, "a": {"_avg": {"_field": "age"}}}
        expected = f"_doc_count={AGG_DOCS} _sum={total} _avg={total / AGG_DOCS:g}"

        def exact(body: dict[str, object] | None) -> bool:
            found = body.get("aggregations") if body else None
            if not isinstance(found, dict):
                return False
            s, a = found.get("s"), found.get("a")
            return (
                found.get("_doc_count") == AGG_DOCS
                and isinstance(s, dict)
                and s.get("_sum") == total
                and isinstance(a, dict)
                and isinstance(a.get("_avg"), int | float)
                and abs(float(a["_avg"]) - total / AGG_DOCS) < 1e-9
            )

        async def on(node: Node, extra: dict[str, object]) -> tuple[bool, str]:
            query: dict[str, object] = {"_query": "*", "_limit": 0, "_aggs": aggs} | extra
            last: list[dict[str, object] | None] = [None]

            async def probe() -> bool | None:
                last[0] = await self.query(node, "cluster_agg", query)
                return True if exact(last[0]) else None

            ok = bool(await until(probe, self.wait))
            got = json.dumps(last[0].get("aggregations") if last[0] else None)[:200]
            return ok, got

        variants: list[dict[str, object]] = [{}, {"_check_at_least": 1}]
        for extra in variants:
            nodes = list(self.cluster.nodes.values())
            results = await asyncio.gather(*(on(node, extra) for node in nodes))
            for node, (ok, got) in zip(nodes, results, strict=True):
                if not ok:
                    item.note(f"{node.name} last aggregations: {got}")
                label = f" with {json.dumps(extra)}" if extra else ""
                item.check(ok, f"{node.name}: {expected}{label}")

    async def failover(self, item: Item) -> None:
        count = len(self.cluster)
        if count == 2:
            item.skipped = "two nodes keep no quorum without the leader; run 1, or 3 and more"
            return
        nodes = list(self.cluster.nodes.values())
        members = frozenset(node.name for node in nodes)
        leader_name = self.agreed(await self.converge(nodes, members, min(self.wait, 10.0)))
        leader = self.cluster.by_name(leader_name) if leader_name else None
        if not item.check(leader is not None, f"the nodes agree on a leader to kill (got {leader_name})"):
            return
        assert leader is not None
        started = time.monotonic()
        await self.cluster.kill9(leader)
        if count == 1:
            relaunched = await self.cluster.restart(leader)
            item.check(relaunched, f"{leader.name} restarts after kill -9")
            views = await self.converge([leader], members, self.wait)
            item.check(self.agreed(views) == leader.name, f"{leader.name} leads again")
            back = await self.read_until(leader, "cluster_probe", "1", self.probe_token, self.wait)
            item.check(back is not None, f"{leader.name} reads cluster_probe/1 after the restart")
            return
        survivors = [node for node in nodes if node is not leader]

        async def new_leader() -> str | None:
            agreed = self.agreed(await self.views(survivors))
            return agreed if agreed is not None and agreed != leader.name else None

        elected = await until(new_leader, self.wait, 1.0)
        took = time.monotonic() - started
        item.check(
            elected is not None,
            f"the survivors elect one new leader after {leader.name} dies ({elected}, {took:.1f} s)",
        )
        writer, reader = survivors[0], survivors[1]

        async def write() -> Response | None:
            put = await self.put(
                f"put@{writer.name}", writer, "cluster_probe", "2", {"title": "after failover", "n": 43}
            )
            return put if put.body is not None else None

        put = await until(write, self.wait, 1.0)
        took = time.monotonic() - started
        item.check(
            put is not None,
            f"a write through {writer.name} succeeds with {leader.name} down ({took:.1f} s after the kill)",
        )
        if put is not None:
            back = await self.read_until(reader, "cluster_probe", "2", put.body, self.wait)
            item.check(back is not None, f"it reads back from {reader.name} with {leader.name} down")
        await self.cluster.launch(leader)
        rejoined = await self.converge(nodes, members, self.wait)
        view = rejoined.get(leader.name)
        item.check(view is not None and members <= view.members, f"{leader.name} rejoins and sees all {count} members")
        item.check(self.agreed(rejoined) is not None, f"one leader after {leader.name} rejoins")

    async def replay(self, item: Item) -> None:
        first = self.cluster.nodes[1]
        tokens: dict[int, Value] = {}
        for k in range(REPLAY_DOCS):
            put = await self.put("put@node1", first, "cluster_replay", f"r{k}", {"age": k})
            tokens[k] = put.body
        item.check(all(tokens.values()), f"{REPLAY_DOCS} uncommitted PUTs on node1")
        await self.cluster.kill9(first)
        restarted = False
        for _ in range(6):  # peers may hold the dead node's name for a while
            await self.cluster.launch(first)
            if await self.cluster.ready(first):
                restarted = True
                break
            await self.cluster.kill9(first)
            await asyncio.sleep(5)
        item.check(restarted, "node1 restarts after kill -9")
        last = REPLAY_DOCS - 1

        async def serves(node: Node) -> tuple[object, bool]:
            totals: list[object] = [None]

            async def probe() -> bool | None:
                body = await self.query(node, "cluster_replay", {"_query": "*", "_limit": 0})
                totals[0] = body.get("total") if body else None
                return True if totals[0] == REPLAY_DOCS else None

            await until(probe, self.wait)
            back = await self.read_until(node, "cluster_replay", f"r{last}", tokens[last], 5)
            return totals[0], back is not None

        nodes = list(self.cluster.nodes.values())
        results = await asyncio.gather(*(serves(node) for node in nodes))
        for node, (total, found) in zip(nodes, results, strict=True):
            item.check(
                total == REPLAY_DOCS, f"{node.name} serves {REPLAY_DOCS} documents after the replay (got {total})"
            )
            item.check(found, f"{node.name} reads the replayed r{last}")

    def history(self, item: Item) -> None:
        self.recorder.file.flush()
        log = history.load_log(self.recorder.path.read_text().splitlines())
        ops = log.ops
        violations = history.check(ops, faults=log.faults)
        for violation in violations[:20]:
            item.note(violation)
        keys = len({op.key for op in ops})
        item.check(
            not violations,
            f"{len(ops)} operations on {keys} keys check at their own levels ({len(violations)} violations)",
        )


async def orphaned(cluster: Cluster, main: asyncio.Task[object]) -> None:
    """Kills the nodes and stops the run when run.sh dies without reaping the group (SIGKILL)."""
    parent = os.getppid()
    while os.getppid() == parent:
        await asyncio.sleep(1)
    print("== run.sh is gone: killing the nodes", flush=True)
    cluster.kill_now()
    main.cancel()


async def run(args: argparse.Namespace, out: Path, missing: list[str]) -> dict[str, Item]:
    recorder = Recorder(out / "history.jsonl")
    cluster = Cluster(args.bin, out, args.nodes, args.node_args, args.join_args, recorder)
    checklist = Checklist(cluster, recorder, args)
    items = {name: Item(name) for name in args.items}
    loop = asyncio.get_running_loop()
    main = asyncio.current_task()
    assert main is not None
    for signum in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
        loop.add_signal_handler(signum, main.cancel)
    watchdog = asyncio.create_task(orphaned(cluster, main))
    try:
        print(f"== cluster checklist: {args.nodes} node(s), {args.bin}, out {out}", flush=True)
        missing += await cluster.start()
        for node in cluster.nodes.values():
            print(
                f"   {node.name}: http {node.http} peer {node.peer} {'(never answered)' if node.name in missing else ''}"
            )
        for name, item in items.items():
            print(f"[{name}]", flush=True)
            try:
                result = getattr(checklist, name)(item)
                if asyncio.iscoroutine(result):
                    await result
            except Exception as error:
                item.error = f"{type(error).__name__}: {error}"
                print(f"  ERROR: {item.error}\n{traceback.format_exc()}", flush=True)
            if item.skipped:
                print(f"  skip: {item.skipped}", flush=True)
    finally:
        watchdog.cancel()
        try:
            await cluster.close()
        finally:
            recorder.close()
    return items


def verdict(items: dict[str, Item], expect_fail: set[str], timed_out: bool, missing: list[str]) -> int:
    failed = {name for name, item in items.items() if item.status == "fail"}
    errors = {name for name, item in items.items() if item.status == "error"}
    print("\n== items: " + ", ".join(f"{name} {item.status}" for name, item in items.items()))
    if timed_out or errors or missing:
        why = "timeout" if timed_out else f"errors in {', '.join(sorted(errors))}" if errors else ""
        why = why or f"{', '.join(missing)} never answered GET /"
        print(f"=== VERDICT: HARNESS ERROR ({why})")
        return 2
    if expect_fail:
        matched = failed == expect_fail
        print(
            f"=== VERDICT: {'AS EXPECTED' if matched else 'UNEXPECTED'}: failed {sorted(failed)}, expected {sorted(expect_fail)}"
        )
        return 0 if matched else 1
    print(f"=== VERDICT: {'GREEN' if not failed else 'RED: ' + ', '.join(sorted(failed))}")
    return 1 if failed else 0


def main(argv: list[str]) -> int:
    repo = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(prog="run.sh check", description="Run the cluster checklist on N local nodes.")
    parser.add_argument("--nodes", type=int, default=3)
    parser.add_argument("--bin", type=Path, default=Path(os.environ.get("BIN", repo / "build/bin/xapiand")))
    parser.add_argument("--out", type=Path, help="run directory (default build/cluster/run-PID)")
    parser.add_argument("--items", default=",".join(ITEMS), help="comma-separated items, in order")
    parser.add_argument("--expect-fail", default="", help="exit 0 only when exactly these items fail")
    parser.add_argument("--wait", type=float, default=30.0, help="seconds each convergence may take")
    parser.add_argument("--timeout", type=float, default=1200.0, help="seconds for the whole run")
    parser.add_argument("--node-args", default="", help="arguments for every node (a template; see cluster.py)")
    parser.add_argument("--join-args", default="", help="arguments for every node but node 1")
    parser.add_argument("--read-level", choices=history.READ_LEVELS, default="prefix")
    parser.add_argument("--read-query", default="", help="appended to every get-by-id, e.g. ?read=linearizable")
    parser.add_argument("--write-level", choices=history.WRITE_LEVELS, default="quorum")
    parser.add_argument("--keep", action="store_true", help="keep the data directories of a green run")
    args = parser.parse_args(argv)
    args.items = [name for name in args.items.split(",") if name]
    expect_fail = {name for name in args.expect_fail.split(",") if name}
    unknown = (set(args.items) | expect_fail) - set(ITEMS)
    if unknown or args.nodes < 1:
        parser.error(f"unknown items {sorted(unknown)}" if unknown else "--nodes must be at least 1")
    if not os.access(args.bin, os.X_OK):
        print(f"missing binary: {args.bin} (build it or pass --bin)", file=sys.stderr)
        return 2
    if os.getpgrp() != os.getpid():
        os.setpgid(0, 0)  # the nodes join this group, and run.sh kills the group
    out = (args.out or repo / "build/cluster" / f"run-{os.getpid()}").resolve()
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    timed_out = False
    items: dict[str, Item] = {}
    missing: list[str] = []

    async def bounded() -> dict[str, Item]:
        async with asyncio.timeout(args.timeout):
            return await run(args, out, missing)

    try:
        items = asyncio.run(bounded())
    except TimeoutError:
        timed_out = True
    except (asyncio.CancelledError, KeyboardInterrupt):
        print("== interrupted", flush=True)
        return 2
    status = verdict(items, expect_fail, timed_out, missing)
    summary = {
        name: {"status": item.status, "error": item.error, "skipped": item.skipped} for name, item in items.items()
    }
    (out / "summary.json").write_text(json.dumps({"nodes": args.nodes, "exit": status, "items": summary}, indent=1))
    if status == 0 and not args.keep:
        for node in out.glob("node*/data"):
            shutil.rmtree(node, ignore_errors=True)
    print(f"== run directory: {out}")
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
