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

"""Tests for the Phase 7c gate tools: async_check's classes, stall_load's open-loop latency and its metrics parsing.

python3 -I harness/test_gate_tools.py        (about 10 s; starts no node)
"""

from __future__ import annotations

import asyncio
import io
import os
import shutil
import sys
import tempfile
import time
import unittest
from contextlib import redirect_stdout
from typing import override

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import async_check
import stall_load


class AsyncCheck(unittest.TestCase):
    @override
    def setUp(self) -> None:
        self.roots: list[str] = []

    @override
    def tearDown(self) -> None:
        for root in self.roots:
            shutil.rmtree(root, ignore_errors=True)

    def tree(self, files: dict[str, str], allowlist: str = "") -> tuple[str, str]:
        root = tempfile.mkdtemp(prefix="asynccheck", dir=os.path.join(os.path.dirname(HERE), ".scratch"))
        self.roots.append(root)
        for rel, text in files.items():
            os.makedirs(os.path.dirname(os.path.join(root, rel)), exist_ok=True)
            with open(os.path.join(root, rel), "w") as fh:
                fh.write(text)
        lst = os.path.join(root, "allow.tsv")
        with open(lst, "w") as fh:
            fh.write(allowlist)
        return root, lst

    def run_check(self, root: str, lst: str, *extra: str) -> tuple[int, str]:
        out = io.StringIO()
        with redirect_stdout(out):
            code = async_check.main(["--root", root, "--allowlist", lst, "--strict", *extra])
        return code, out.getvalue()

    def test_unclassified_hit_fails_and_a_row_classifies_it(self) -> None:
        src = "void f() { std::lock_guard<std::mutex> lk(m); }\n"
        root, lst = self.tree({"src/a.cc": src})
        code, out = self.run_check(root, lst)
        self.assertEqual(code, 1)
        self.assertIn("src/a.cc:1: [mutex] unclassified", out)
        with open(lst, "w") as fh:
            fh.write("src/a.cc\tmutex\t.\tallow\t1\tmap lookup under the lock\n")
        code, out = self.run_check(root, lst)
        self.assertEqual(code, 0, out)

    def test_a_new_hit_in_a_covered_file_fails(self) -> None:
        one = "void f() { std::lock_guard<std::mutex> lk(m); }\n"
        root, lst = self.tree({"src/a.cc": one + one}, "src/a.cc\tmutex\t.\tallow\t1\tone lookup\n")
        code, out = self.run_check(root, lst)
        self.assertEqual(code, 1)
        self.assertIn("a new hit needs its own row", out)

    def test_every_rule_class_is_found(self) -> None:
        lines = [
            "auto x = engine::sync_wait(t);",
            "cv.wait(lk);",
            "std::this_thread::sleep_for(1ms);",
            "std::this_thread::yield();",
            "int fd = ::open(p, 0);",
            "std::ifstream f(p);",
            "lock_shard lk_shard(endpoint, flags);",
            "DatabaseWAL wal(path);",
        ]
        root, lst = self.tree({"src/a.cc": "\n".join(lines) + "\n"})
        _code, out = self.run_check(root, lst)
        for rule in ("sync_wait", "wait", "sleep", "file", "pool", "wal"):
            self.assertIn(f"[{rule}] unclassified", out)
        self.assertEqual(out.count("unclassified:"), len(lines))

    def test_comments_strings_and_declarations_are_not_hits(self) -> None:
        text = '// std::lock_guard<std::mutex> lk(m);\nauto s = "sync_wait";\nstd::mutex mtx;\n#include <condition_variable>\n'
        root, lst = self.tree({"src/a.cc": text})
        code, out = self.run_check(root, lst)
        self.assertEqual(code, 0, out)

    def test_never_open_paths_are_skipped(self) -> None:
        bad = "void f() { std::lock_guard<std::mutex> lk(m); }\n"
        root, lst = self.tree(
            {
                "src/xapian/x.cc": bad,
                "src/engine/xapian/y.cc": bad,
                "src/sortable_serialise.cc": bad,
                "src/server/remote_protocol_views.cc": bad,
                "src/server/replication_protocol_views.cc": bad,
            }
        )
        code, out = self.run_check(root, lst)
        self.assertEqual(code, 0, out)
        self.assertIn("0 hits", out)

    def test_a_removal_shrinks_a_row_without_failing(self) -> None:
        root, lst = self.tree({"src/a.cc": "int x;\n"}, "src/a.cc\tmutex\t.\towed\t2\tD5: the lease replaces it\n")
        code, out = self.run_check(root, lst)
        self.assertEqual(code, 0, out)
        self.assertIn("shrunk", out)

    def test_an_owed_row_must_name_its_decision(self) -> None:
        root, lst = self.tree({"src/a.cc": "int x;\n"}, "src/a.cc\tmutex\t.\towed\t1\tsoon\n")
        with self.assertRaises(SystemExit):
            self.run_check(root, lst)

    def test_the_tree_has_no_owed_hit(self) -> None:
        """The gate: Xapiand's own sources pass `--strict --owed` (the strata rows need --extern, which CI passes)."""
        out = io.StringIO()
        with redirect_stdout(out):
            code = async_check.main(["--strict", "--owed"])
        self.assertEqual(code, 0, out.getvalue())
        self.assertIn("owed 0", out.getvalue())

    def test_owed_hits_fail_only_with_the_owed_flag(self) -> None:
        src = "void f() { std::lock_guard<std::mutex> lk(m); }\n"
        root, lst = self.tree({"src/a.cc": src}, "src/a.cc\tmutex\t.\towed\t1\tD5: the lease replaces it\n")
        self.assertEqual(self.run_check(root, lst)[0], 0)
        self.assertEqual(self.run_check(root, lst, "--owed")[0], 1)


class FakeServer:
    """An HTTP/1.1 server that answers every request after `service_s`, one request at a time per connection."""

    def __init__(self, service_s: float) -> None:
        self.service_s = service_s
        self.server: asyncio.AbstractServer | None = None
        self.port = 0

    async def start(self) -> None:
        self.server = await asyncio.start_server(self.handle, "127.0.0.1", 0)
        self.port = self.server.sockets[0].getsockname()[1]

    async def handle(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        try:
            while True:
                head = await reader.readuntil(b"\r\n\r\n")
                length = 0
                for line in head.split(b"\r\n"):
                    if line.lower().startswith(b"content-length:"):
                        length = int(line.split(b":")[1])
                if length:
                    await reader.readexactly(length)
                await asyncio.sleep(self.service_s)
                writer.write(b"HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}")
                await writer.drain()
        except (asyncio.IncompleteReadError, ConnectionError):
            pass
        finally:
            writer.close()

    async def stop(self) -> None:
        assert self.server is not None
        self.server.close()
        await self.server.wait_closed()


class StallLoad(unittest.TestCase):
    def light(self, service_s: float, rate: float, conns: int, drain: float = 5.0) -> stall_load.LightResult:
        async def go() -> stall_load.LightResult:
            fake = FakeServer(service_s)
            await fake.start()
            try:
                rows = stall_load.Rows([b'{"contact":{"state":"X"},"_x":1}'], ["X"])
                return await stall_load.run_light("127.0.0.1", fake.port, rows, 100, rate, 2.0, conns, 1, 0.0, drain)
            finally:
                await fake.stop()

        return asyncio.run(go())

    def test_latency_runs_from_the_scheduled_time(self) -> None:
        # One connection serves 50 requests a second and 60 are scheduled, so the queue grows to about 0.4 s by the
        # end. Taken from the send time every latency would read about 20 ms; from the scheduled time the queue shows.
        res = self.light(0.02, 60.0, 1, drain=5.0)
        self.assertEqual(res.unfinished, 0)
        self.assertGreater(res.as_dict()["all"]["p99_us"], 250_000)

    def test_a_backlog_left_at_the_cutoff_is_counted_at_its_age(self) -> None:
        res = self.light(0.05, 100.0, 1, drain=0.5)
        self.assertGreater(res.unfinished, 0)
        self.assertGreater(res.as_dict()["all"]["p99_us"], 500_000)

    def test_an_unloaded_server_reads_its_service_time(self) -> None:
        res = self.light(0.005, 50.0, 8)
        q = res.as_dict()["all"]
        self.assertLess(q["p50_us"], 30_000)
        self.assertGreaterEqual(q["p50_us"], 5_000)
        self.assertEqual(res.unfinished, 0)
        d = res.as_dict()
        self.assertLessEqual(d["rate_error"], 0.05)

    def test_a_generator_that_cannot_keep_up_misses_the_rate(self) -> None:
        # 1,000,000 requests a second is more than one Python loop can issue; the rate error says so.
        async def go() -> stall_load.LightResult:
            fake = FakeServer(0.001)
            await fake.start()
            try:
                rows = stall_load.Rows([b'{"contact":{"state":"X"},"_x":1}'], ["X"])
                return await stall_load.run_light("127.0.0.1", fake.port, rows, 100, 1_000_000.0, 0.05, 4, 1, 0.0, 0.2)
            finally:
                await fake.stop()

        res = asyncio.run(go())
        self.assertGreater(res.as_dict()["rate_error"], 0.05)

    METRICS = (
        "# TYPE xapiand_reactor_lag_seconds histogram\n"
        'xapiand_reactor_lag_seconds_bucket{cluster="c",node="n",loop="http0",le="0.000128"} 10\n'
        'xapiand_reactor_lag_seconds_bucket{cluster="c",node="n",loop="http0",le="0.001024"} 98\n'
        'xapiand_reactor_lag_seconds_bucket{cluster="c",node="n",loop="http0",le="0.005120"} 100\n'
        'xapiand_reactor_lag_seconds_bucket{cluster="c",node="n",loop="http0",le="+Inf"} 100\n'
        'xapiand_reactor_lag_seconds_sum{cluster="c",node="n",loop="http0"} 0.01\n'
        'xapiand_reactor_lag_seconds_count{cluster="c",node="n",loop="http0"} 100\n'
        'xapiand_reactor_lag_max_seconds{cluster="c",node="n",loop="http0"} 0.004\n'
        'xapiand_servers_running{cluster="c",node="n"} 10\n'
    )

    def test_metrics_parse_and_lag_delta_read_bucket_edges(self) -> None:
        before = stall_load.parse_metrics(
            'xapiand_reactor_lag_seconds_bucket{loop="http0",le="0.000128"} 0\n'
            'xapiand_reactor_lag_seconds_bucket{loop="http0",le="0.001024"} 8\n'
            'xapiand_reactor_lag_seconds_bucket{loop="http0",le="0.005120"} 10\n'
            'xapiand_reactor_lag_seconds_bucket{loop="http0",le="+Inf"} 10\n'
        )
        after = stall_load.parse_metrics(self.METRICS)
        self.assertEqual(after["gauges"]["xapiand_servers_running"], 10)
        delta = stall_load.lag_delta(before, after)["http0"]
        self.assertEqual(delta["wakes"], 90)
        self.assertEqual(delta["p99_us"], 1024.0)
        self.assertTrue(delta["p99_within_1ms"])
        self.assertEqual(delta["max_us_so_far"], 4000.0)
        whole = stall_load.lag_delta({"lag": {}, "gauges": {}}, after)["http0"]
        self.assertEqual(whole["p99_us"], 5120.0)
        self.assertEqual(whole["p50_us"], 1024.0)
        self.assertTrue(whole["p99_within_1ms"] is False)

    def test_percentiles(self) -> None:
        values = list(range(1, 1001))
        self.assertEqual(stall_load.pct(values, 50), 500.0)
        self.assertEqual(stall_load.pct(values, 99), 990.0)
        self.assertEqual(stall_load.pct(values, 99.9), 999.0)
        self.assertIsNone(stall_load.pct([], 50))

    def test_a_refusal_reads_its_cause(self) -> None:
        engine = b'{"status":503,"type":"Service Unavailable","message":"Service Unavailable: The index is busy"}'
        self.assertEqual(stall_load.refusal_reason(engine), "Service Unavailable: The index is busy")
        self.assertEqual(stall_load.refusal_reason(b'{"message":["a","b"]}'), "a; b")
        self.assertEqual(stall_load.refusal_reason(b"The server is busy\n"), "The server is busy")
        self.assertEqual(stall_load.refusal_reason(b"[1]"), "[1]")

    def test_a_process_cpu_time_reads_in_seconds(self) -> None:
        spent = time.process_time()
        while time.process_time() - spent < 0.3:
            pass
        cpu = stall_load.process_cpu_s(os.getpid())
        assert cpu is not None
        self.assertGreaterEqual(cpu, 0.25)
        self.assertLess(cpu, time.process_time() + 1.0)


class CappedServer:
    """An HTTP/1.1 server with the reactor gate's admission: a request is admitted at headers-complete and held
    until its response; over `cap` (None: no cap) it is refused with a 503 and `Connection: close`."""

    def __init__(self, cap: int | None, service_s: float = 0.0) -> None:
        self.cap, self.service_s = cap, service_s
        self.inflight = self.peak = 0
        self.server: asyncio.AbstractServer | None = None
        self.port = 0

    async def start(self) -> None:
        self.server = await asyncio.start_server(self.handle, "127.0.0.1", 0, backlog=512)
        self.port = self.server.sockets[0].getsockname()[1]

    async def handle(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        try:
            while True:
                head = await reader.readuntil(b"\r\n\r\n")
                if self.cap is not None and self.inflight >= self.cap:
                    writer.write(b"HTTP/1.1 503 Service Unavailable\r\nContent-Length: 20\r\nRetry-After: 1\r\n")
                    writer.write(b"Connection: close\r\n\r\nService Unavailable\n")
                    await writer.drain()
                    return
                self.inflight += 1
                self.peak = max(self.peak, self.inflight)
                try:
                    length = 0
                    for line in head.split(b"\r\n"):
                        if line.lower().startswith(b"content-length:"):
                            length = int(line.split(b":")[1])
                    if length:
                        await reader.readexactly(length)
                    await asyncio.sleep(self.service_s)
                    writer.write(b"HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}")
                    await writer.drain()
                finally:
                    self.inflight -= 1
        except (asyncio.IncompleteReadError, ConnectionError):
            pass
        finally:
            writer.close()

    async def stop(self) -> None:
        assert self.server is not None
        self.server.close()
        await self.server.wait_closed()


HELD_REQUESTS = [("search", "SEARCH", "/accounts/", b'{"_query":{"contact.state":"X"}}')]


class HeldOverload(unittest.TestCase):
    def held(self, cap: int | None, held: int, service_s: float = 0.0) -> tuple[stall_load.HeldResult, int]:
        async def go() -> tuple[stall_load.HeldResult, int]:
            fake = CappedServer(cap, service_s)
            await fake.start()
            try:
                res = await stall_load.run_held(
                    "127.0.0.1", fake.port, HELD_REQUESTS, held, cap or 40, 0.5, barrier_timeout=1.0
                )
                return res, fake.peak
            finally:
                await fake.stop()

        return asyncio.run(go())

    def test_a_server_that_answers_at_once_still_has_its_cap_exceeded(self) -> None:
        # An open-loop stream at any rate this server keeps up with leaves about one request in flight; held heads
        # put `held` in flight whatever the speed, so the cap admits 40 and refuses 20 in every round.
        res, peak = self.held(40, 60)
        d = res.as_dict()
        self.assertTrue(res.enforced)
        self.assertEqual(peak, 40)
        for r in res.rounds:
            self.assertEqual(r["refused_at_barrier"], 20)
            self.assertEqual(r["in_flight_at_barrier"], 40)
        self.assertEqual(d["errors"]["http_503_by_reason"], {"Service Unavailable": 20 * len(res.rounds)})
        self.assertEqual(d["answered"]["n"], 40 * len(res.rounds))
        self.assertEqual(d["errors"]["exceptions"], 0)

    def test_a_server_without_a_cap_is_not_enforced(self) -> None:
        res, peak = self.held(None, 60)
        self.assertFalse(res.enforced)
        self.assertEqual(peak, 60)
        self.assertEqual(res.as_dict()["errors"]["http_503"], 0)


if __name__ == "__main__":
    unittest.main()
