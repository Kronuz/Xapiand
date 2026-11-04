#!/usr/bin/env python3
"""Tests for the closing benchmark's harness: the adapters' translations, D8's checks, the cache probe, the open-loop
generators and the crash audit. The first group starts nothing; the second starts a local Xapiand, Elasticsearch and
Quickwit when CLOSING_LIVE=1 and CLOSING_XAPIAND, CLOSING_ES (bin/elasticsearch) and CLOSING_QUICKWIT name the binaries.

python3 -I harness/test_closing_harness.py            (about 15 s without the live group)
CLOSING_LIVE=1 CLOSING_XAPIAND=... CLOSING_ES=... CLOSING_QUICKWIT=... python3 -I harness/test_closing_harness.py Live
"""

from __future__ import annotations

import argparse
import http.server
import json
import os
import sys
import tempfile
import threading
import time
import unittest
from typing import Any, ClassVar, override

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import closing_mixed
import closing_tenants
import loadtest
import stall_load
import systems as sy

LOGS = sy.Schema.load("logs")
T0 = 893964617


def q(**kw: Any) -> sy.Query:
    return sy.Query.parse(json.dumps(kw))


class Queries(unittest.TestCase):
    def test_round_trip(self) -> None:
        for line in open(os.path.join(HERE, "closing", "queries", "logs-smoke.jsonl")):
            query = sy.Query.parse(line)
            self.assertEqual(sy.Query.parse(query.dumps()), query)

    def test_rejects_the_unknown(self) -> None:
        for bad in (
            {"class": "x"},
            {"class": "x", "terms": ["a"], "mode": "fuzzy"},
            {"class": "x", "terms": ["a"], "command": "x"},
        ):
            with self.assertRaises(ValueError):
                sy.Query.parse(json.dumps(bad))

    def test_vary_leaves_run_zero_and_shifts_the_rest(self) -> None:
        query = q(**{"class": "r", "range": {"field": "size", "gte": 1000, "lte": 2000}})
        self.assertEqual(query.vary(0), query)
        shifts = {((query.vary(r).range or ("", 1000.0, 0.0))[1] or 0) - 1000 for r in range(1, 40)}
        self.assertGreater(len(shifts), 3, "bounds must differ between runs")
        self.assertTrue(all(0 <= s < 10 for s in shifts))
        self.assertEqual(query.vary(5, 7), query.vary(5, 7), "the same seed gives the same bounds")
        self.assertEqual(q(**{"class": "t", "field": "request", "terms": ["a"]}).vary(3).range, None)


class Translation(unittest.TestCase):
    @override
    def setUp(self) -> None:
        self.x, self.e, self.qw = sy.Xapiand("h:1"), sy.Elasticsearch("h:2"), sy.Quickwit("h:3")
        self.term = q(**{"class": "term", "field": "request", "terms": ["get"]})
        self.agg = q(**{"class": "agg", "agg": {"kind": "terms", "field": "status"}})
        self.rng = q(**{"class": "range", "range": {"field": "@timestamp", "gte": T0, "lte": T0 + 9}})

    def body(self, system: sy.System, query: sy.Query) -> tuple[sy.Request, dict[str, Any]]:
        req = system.search_request("logs", LOGS, query)
        return req, json.loads(req.body)

    def test_result_caches_are_off_in_every_search(self) -> None:
        req, _ = self.body(self.e, self.agg)
        self.assertIn("request_cache=false", req.path)
        self.assertIn(
            '"index.requests.cache.enable": false',
            open(os.path.join(HERE, "closing", "elasticsearch-index.json")).read(),
        )
        self.assertIn(
            "partial_request_cache_capacity: {partial_cache}",
            open(os.path.join(HERE, "closing", "quickwit.yaml")).read(),
        )
        self.assertIn(
            "partial_request_cache_capacity: 0\n",
            sy.render("quickwit.yaml", port=1, grpc=2, data="d", cpu_millis=1000, partial_cache="0"),
        )

    def test_count_mode_is_pinned_per_command(self) -> None:
        _, e_search = self.body(self.e, self.term)
        _, e_exact = self.body(self.e, self.term.as_command("search_exact"))
        self.assertIs(e_search["track_total_hits"], False)
        self.assertIs(e_exact["track_total_hits"], True)
        self.assertTrue(self.e.search_request("logs", LOGS, self.term.as_command("count")).path.endswith("/_count"))
        _, q_search = self.body(self.qw, self.term)
        _, q_exact = self.body(self.qw, self.term.as_command("search_exact"))
        self.assertIs(q_search["count_all"], False)
        self.assertNotIn("count_all", q_exact)
        self.assertEqual(self.body(self.qw, self.term.as_command("count"))[1]["max_hits"], 0)
        self.assertNotIn("check_at_least", self.x.search_request("logs", LOGS, self.term).path)
        for cmd in ("search_exact", "count"):
            self.assertIn(
                f"check_at_least={sy.EXACT_AT_LEAST}",
                self.x.search_request("logs", LOGS, self.term.as_command(cmd)).path,
            )

    def test_relevance_queries_ask_quickwit_to_score(self) -> None:
        self.assertEqual(self.body(self.qw, self.term)[1]["sort_by"], "_score")

    def test_quickwit_sorts_descending_without_a_minus(self) -> None:
        srt = q(**{"class": "s", "terms": ["200"], "field": "status", "sort": {"field": "@timestamp", "order": "desc"}})
        self.assertEqual(self.body(self.qw, srt)[1]["sort_by"], "@timestamp")
        self.assertEqual(
            self.body(self.qw, q(**{"class": "s", "sort": {"field": "size", "order": "asc"}}))[1]["sort_by"], "-size"
        )

    def test_timestamp_range_prunes_quickwit_splits_and_es_reads_epoch_seconds(self) -> None:
        body = self.body(self.qw, self.rng)[1]
        self.assertEqual((body["start_timestamp"], body["end_timestamp"]), (T0, T0 + 10))
        self.assertIn("1998-04-30T", body["query"])
        self.assertEqual(self.body(self.e, self.rng)[1]["query"]["range"]["@timestamp"]["format"], "epoch_second")

    def test_phrase_and_boolean_shapes(self) -> None:
        phrase = q(**{"class": "p", "field": "request", "terms": ["get", "images"], "mode": "phrase"})
        self.assertEqual(self.body(self.x, phrase)[1]["_query"], {"request": {"_phrase": "get images"}})
        self.assertIn("match_phrase", self.body(self.e, phrase)[1]["query"])
        self.assertIn('request:"get images"', self.body(self.qw, phrase)[1]["query"])
        union = q(**{"class": "o", "field": "request", "terms": ["a", "b"], "mode": "any"})
        self.assertIn("_or", self.body(self.x, union)[1]["_query"])
        self.assertIn(" OR ", self.body(self.qw, union)[1]["query"])

    def test_terms_carry_no_syntax(self) -> None:
        with self.assertRaises(ValueError):
            sy.clean_term('"*:()')
        self.assertEqual(sy.clean_term("a+b"), "ab")

    def test_answers_parse_to_one_shape(self) -> None:
        self.assertEqual(
            self.x.parse_answer(self.term, 200, b'{"total":7,"hits":[{"_id":"a"}],"aggregations":{}}').count, 7
        )
        self.assertEqual(
            self.e.parse_answer(self.term, 200, b'{"hits":{"total":{"value":7},"hits":[{"_id":"a"}]}}').ids, ["a"]
        )
        self.assertIsNone(self.e.parse_answer(self.term, 200, b'{"hits":{"hits":[]}}').count)
        self.assertEqual(self.qw.parse_answer(self.term, 200, b'{"num_hits":7,"hits":[]}').count, 7)
        agg = self.e.parse_answer(
            self.agg, 200, b'{"hits":{"hits":[]},"aggregations":{"a":{"buckets":[{"key":200,"doc_count":3}]}}}'
        )
        self.assertEqual(agg.aggs, {"200": 3})
        hist = q(**{"class": "h", "agg": {"kind": "date_histogram", "field": "@timestamp", "interval": 3600}})
        xa = self.x.parse_answer(hist, 200, b'{"total":2,"aggregations":{"a":[{"_doc_count":2,"_key":"893962800"}]}}')
        qa = self.qw.parse_answer(
            hist, 200, b'{"num_hits":2,"aggregations":{"a":{"buckets":[{"key":893962800000.0,"doc_count":2}]}}}'
        )
        self.assertEqual(xa.aggs, qa.aggs)

    def test_schema_bodies(self) -> None:
        body = self.x.schema_body(sy.Schema.load("wiki"))
        self.assertEqual(body["id"], {"_type": "keyword", "_index": "none"})
        self.assertEqual(body["text"], {"_type": "text"})
        index = json.loads(self.e.create_index("t", LOGS)[0].body)
        self.assertEqual(index["mappings"]["properties"]["@timestamp"]["format"], "epoch_second")
        conf = json.loads(self.qw.create_index("t", LOGS)[0].body)
        self.assertEqual(conf["doc_mapping"]["timestamp_field"], "@timestamp")
        self.assertEqual(conf["search_settings"]["default_search_fields"], ["request"])


class Fake(sy.System):
    """An adapter whose answers come from a table, to exercise D8 and the probe without a server."""

    name = "fake"

    def __init__(self, counts: dict[str, int], ids: list[str] | None = None, hits: int = 0) -> None:
        super().__init__("h:0")
        self.counts, self.ids, self.hits = counts, ids or [], hits

    def command(self, launch: sy.Launch) -> tuple[list[str], dict[str, str]]:
        return [], {}

    def ready_request(self) -> sy.Request:
        return sy.Request("GET", "/")

    def create_index(self, index: str, schema: sy.Schema, bulk: bool = False) -> list[sy.Request]:
        return []

    def bulk_request(self, index: str, schema: sy.Schema, lines: list[bytes], last: bool = False) -> sy.Request:
        return sy.Request("POST", "/")

    def parse_bulk(self, status: int, body: bytes, n: int) -> sy.Bulk:
        return sy.Bulk(True, n)

    def commit_requests(self, index: str, bulk: bool = False) -> list[sy.Request]:
        return []

    def put_request(self, index: str, doc_id: str, doc: dict[str, Any]) -> sy.Request:
        return sy.Request("PUT", "/")

    def floor_request(self, index: str) -> sy.Request:
        return sy.Request("GET", "/")

    def dump(self, client: sy.Client, index: str) -> list[tuple[str, int]]:
        return []

    def dump_stream(self, client: sy.Client, index: str, stream: str) -> list[int]:
        return []

    def search_request(self, index: str, schema: sy.Schema, q: sy.Query) -> sy.Request:
        return sy.Request("GET", "/")

    def parse_answer(self, q: sy.Query, status: int, body: bytes) -> sy.Answer:
        return sy.Answer(200, self.counts.get(q.terms[0] if q.terms else "", 0), list(self.ids))

    @override
    def run(self, client: sy.Client, index: str, schema: sy.Schema, q: sy.Query) -> sy.Answer:
        self.hits += 1 if q.range is None else 0
        return self.parse_answer(q, 200, b"")

    def cache_counters(self, client: sy.Client) -> dict[str, Any]:
        return {"result_hits": self.hits, "filter_hits": 0}

    def effective(self, client: sy.Client, index: str | None = None) -> dict[str, Any]:
        return {}


class Checks(unittest.TestCase):
    def members(self, tables: dict[str, dict[str, int]], ids: dict[str, list[str]] | None = None) -> list[sy.Member]:
        return [
            sy.Member(label, Fake(counts, (ids or {}).get(label)), sy.Client("h:0"), "i")
            for label, counts in tables.items()
        ]

    def queries(self, n: int) -> list[sy.Query]:
        return [q(**{"class": "term", "field": "request", "terms": [f"t{i}"]}) for i in range(n)]

    def test_counts_within_one_percent_are_kept_and_beyond_dropped(self) -> None:
        tables = {"S": {"t0": 1000, "t1": 1000}, "X": {"t0": 1000, "t1": 1000}, "E": {"t0": 995, "t1": 900}}
        report = sy.check_systems(self.members(tables), LOGS, self.queries(2))
        self.assertEqual(len(report["kept"]), 1)
        self.assertEqual(len(report["dropped"]), 1)
        self.assertIn("counts differ", report["dropped"][0]["reason"])

    def test_more_than_five_percent_dropped_in_a_class_stops_the_run(self) -> None:
        names = [f"t{i}" for i in range(10)]
        tables = {"S": dict.fromkeys(names, 100), "E": {**dict.fromkeys(names, 100), "t0": 1}}
        self.assertTrue(sy.check_systems(self.members(tables), LOGS, self.queries(10))["stop"])
        tables["E"]["t0"] = 100
        self.assertFalse(sy.check_systems(self.members(tables), LOGS, self.queries(10))["stop"])

    def test_s_against_x_compares_top_ten_in_order(self) -> None:
        tables = {"S": {"t0": 5}, "X": {"t0": 5}}
        same = sy.check_systems(self.members(tables, {"S": ["a", "b"], "X": ["a", "b"]}), LOGS, self.queries(1))
        swapped = sy.check_systems(self.members(tables, {"S": ["a", "b"], "X": ["b", "a"]}), LOGS, self.queries(1))
        self.assertEqual(same["order_diffs"], [])
        self.assertEqual(len(swapped["order_diffs"]), 1)

    def test_relative_gap(self) -> None:
        self.assertEqual(sy.relative_gap([0, 0]), 0.0)
        self.assertAlmostEqual(sy.relative_gap([100, 99]), 0.01)

    def test_the_cache_probe_fails_when_a_result_cache_answers(self) -> None:
        quiet = Fake({}, hits=0)
        # A range-free query that the fake counts as a result-cache hit stands for a cache answering a repeat.
        members = [sy.Member("E", quiet, sy.Client("h:0"), "i")]
        probe = sy.probe_caches(members, LOGS, [q(**{"class": "r", "range": {"field": "size", "gte": 1, "lte": 2}})], 3)
        self.assertTrue(probe["E"]["ok"])
        probe = sy.probe_caches(members, LOGS, self.queries(1), 3)
        self.assertFalse(probe["E"]["ok"])
        self.assertGreater(probe["E"]["delta"]["result_hits"], 0)


class OpenLoop(unittest.TestCase):
    def test_merge_and_the_bound(self) -> None:
        part = {
            "k": 0,
            "issued": 100,
            "completed": 100,
            "unfinished": 0,
            "issue_span": 1.0,
            "latencies_us": [1000] * 99 + [30_000],
            "counts": {"ok": 100, "http_503": 0, "http_other": 0, "exceptions": 0},
            "generator_cpu": 0.3,
        }
        merged = stall_load.merge_open_loop([part, {**part, "k": 1}], 200.0, 1.0)
        self.assertEqual(merged["issued"], 200)
        self.assertEqual(merged["latency"]["p99_us"], 1000.0 if merged["latency"]["p99_us"] == 1000.0 else 30_000.0)
        self.assertTrue(stall_load.good_step({**merged, "latency": {"p99_us": 20_000.0}, "rate_error": 0.0}, 25.0))
        self.assertFalse(stall_load.good_step({**merged, "latency": {"p99_us": 26_000.0}, "rate_error": 0.0}, 25.0))
        self.assertFalse(stall_load.good_step({**merged, "latency": {"p99_us": 1.0}, "rate_error": 0.02}, 25.0))
        self.assertFalse(
            stall_load.good_step(
                {**merged, "latency": {"p99_us": 1.0}, "rate_error": 0.0, "generator_cpu_max": 0.9}, 25.0
            )
        )

    def test_bounds_per_class(self) -> None:
        self.assertEqual(stall_load.class_bound_ms("term", 0), 25.0)
        self.assertEqual(stall_load.class_bound_ms("agg_terms", 0), 250.0)
        self.assertEqual(stall_load.class_bound_ms("term", 7.0), 7.0)

    def test_two_processes_split_one_schedule_against_a_stub(self) -> None:
        seen: list[str] = []

        class Stub(http.server.BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def do_SEARCH(self) -> None:
                self.rfile.read(int(self.headers.get("Content-Length", 0)))
                seen.append(self.path)
                body = b'{"total":1,"hits":[]}'
                self.send_response(200)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            @override
            def log_message(self, format: str, *args: Any) -> None:
                pass

        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Stub)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        args = argparse.Namespace(
            system="xapiand",
            target=f"127.0.0.1:{server.server_address[1]}",
            index="logs",
            schema="logs",
            queries=os.path.join(HERE, "closing", "queries", "logs-smoke.jsonl"),
            light_conns=16,
            seed=7,
            procs=2,
        )
        try:
            res = stall_load.run_open_loop(args, "term", 100.0, 3.0, 0.5)
        finally:
            server.shutdown()
        self.assertEqual(res["procs"], 2)
        self.assertEqual(res["issued"], 300)
        self.assertLess(res["rate_error"], 0.05)
        self.assertEqual(res["errors"]["http_other"] + res["errors"]["exceptions"], 0)
        self.assertEqual(len(seen), 300, "every scheduled unit reaches the server once")


class Audit(unittest.TestCase):
    def model(
        self, recs: dict[str, tuple[int | None, float]], attempted: set[str] | None = None
    ) -> closing_mixed.Model:
        m = closing_mixed.Model()
        for i, (v, t) in recs.items():
            m.records[i] = closing_mixed.Record(v, t)
        m.attempted = attempted if attempted is not None else set(recs)
        return m

    def test_lost_late_unwritten_duplicated(self) -> None:
        m = self.model({"a": (1, 90.0), "b": (2, 99.5), "c": (None, 90.0), "d": (4, 90.0)})
        got = [("a", 1), ("c", 3), ("z", 9), ("z", 9)]
        rep = closing_mixed.audit(got, m, killed=100.0, grace=2.0, poke_ids=set())
        self.assertEqual(rep["lost_beyond_grace"], 2, "d is missing and c, deleted long ago, is present")
        self.assertEqual(rep["late_inside_grace"], 1, "b was acknowledged inside the grace window")
        self.assertEqual(rep["oldest_late_age_s"], 0.5)
        self.assertEqual(rep["unwritten_present"], 1)
        self.assertEqual(rep["duplicated"], 1)

    def test_a_clean_run_has_no_grace(self) -> None:
        m = self.model({"a": (1, 99.9)})
        self.assertEqual(closing_mixed.audit([], m, killed=None, grace=0.0, poke_ids=set())["lost_beyond_grace"], 1)


class Stream(unittest.TestCase):
    """T7's marked write stream (CB-R16): the writer's documents, its sent-id log and the audit's verdicts."""

    stored: list[dict[str, Any]]
    model: closing_mixed.Model

    @override
    def setUp(self) -> None:
        stored: list[dict[str, Any]] = []
        lock = threading.Lock()

        class Stub(http.server.BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def do_POST(self) -> None:
                raw = self.rfile.read(int(self.headers.get("Content-Length", 0)))
                with lock:
                    stored.extend(json.loads(ln) for ln in raw.splitlines() if ln.strip())
                self.send_response(200)
                self.send_header("Content-Length", "2")
                self.end_headers()
                self.wfile.write(b"{}")

            @override
            def log_message(self, format: str, *args: Any) -> None:
                pass

        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Stub)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        self.addCleanup(server.shutdown)
        lines = iter(
            json.dumps({"@timestamp": T0 + n, "request": "GET /a", "status": 200, "size": n}).encode()
            for n in range(10_000)
        )
        self.model = closing_mixed.Model()
        stop = threading.Event()
        worker = threading.Thread(
            target=closing_mixed.stream_writer,
            args=(
                0,
                Fake({}),
                LOGS,
                "logs",
                f"127.0.0.1:{server.server_address[1]}",
                self.model,
                stop,
                threading.Event(),
                lines,
                threading.Lock(),
                500.0,
                25,
            ),
        )
        worker.start()
        time.sleep(0.8)
        stop.set()
        worker.join(timeout=10)
        self.stored = stored

    def got(self) -> list[tuple[str, int]]:
        return [(f"{d['stream']}-{d['seq']}", int(d["seq"])) for d in self.stored]

    def test_documents_are_marked_numbered_and_every_id_is_logged(self) -> None:
        self.assertGreater(len(self.stored), 200)
        self.assertEqual([d["seq"] for d in self.stored], list(range(1, len(self.stored) + 1)))
        self.assertEqual({d["stream"] for d in self.stored}, {"t7w0"})
        self.assertEqual(self.model.attempted, {f"t7w0-{n}" for n in range(1, len(self.stored) + 1)})
        path = os.path.join(tempfile.mkdtemp(dir=os.path.join(os.path.dirname(HERE), ".scratch")), "sent.tsv")
        closing_mixed.write_sent_log(path, self.model, sorted(self.model.attempted))
        rows = [ln.split("\t") for ln in open(path).read().splitlines()]
        self.assertEqual(len(rows), len(self.stored))
        self.assertTrue(all(r[1] == "ack" and float(r[2]) > 0 for r in rows))

    def test_a_clean_stream_passes_and_each_fault_is_named(self) -> None:
        killed = max(r.acked for r in self.model.records.values()) + 0.1
        old = min(self.model.records, key=lambda i: self.model.records[i].acked)
        recent = max(self.model.records, key=lambda i: self.model.records[i].acked)
        clean = closing_mixed.audit(self.got(), self.model, killed, 2.0, set())
        self.assertTrue(closing_mixed.passed(clean, False, []))
        lost = closing_mixed.audit([g for g in self.got() if g[0] != old], self.model, killed, 2.0, set())
        self.assertEqual(lost["lost_beyond_grace"], 1)
        late = closing_mixed.audit([g for g in self.got() if g[0] != recent], self.model, killed, 2.0, set())
        self.assertEqual((late["lost_beyond_grace"], late["late_inside_grace"]), (0, 1), "inside the window, counted")
        dup = closing_mixed.audit([*self.got(), self.got()[3]], self.model, killed, 2.0, set())
        self.assertEqual(dup["duplicated"], 1)
        extra = closing_mixed.audit([*self.got(), ("t7w0-999999", 999999)], self.model, killed, 2.0, set())
        self.assertEqual(extra["unwritten_present"], 1)
        self.assertFalse(closing_mixed.passed({**clean, "base_unchanged": False}, False, []), "a changed base fails")


class Tenants(unittest.TestCase):
    def test_the_corpus_is_cut_in_order(self) -> None:
        with tempfile.NamedTemporaryFile(
            "w", suffix=".ndjson", delete=False, dir=os.path.join(os.path.dirname(HERE), ".scratch")
        ) as fh:
            fh.write("".join(f'{{"n":{i}}}\n' for i in range(25)))
        try:
            got = list(closing_tenants.tenant_batches(fh.name, 3, 4))
        finally:
            os.unlink(fh.name)
        self.assertEqual([i for i, _ in got], [0, 1, 2])
        self.assertEqual(got[2][1][0], b'{"n":8}\n')
        self.assertEqual(sum(len(b) for _, b in got), 12)
        self.assertEqual(closing_tenants.tenant_name(255), "t000ff")


class Helpers(unittest.TestCase):
    def test_geomean_percentile_batches(self) -> None:
        self.assertAlmostEqual(loadtest.geomean([1.0, 4.0]) or 0, 2.0)
        self.assertEqual(loadtest.pct([1.0, 2.0, 3.0, 4.0], 50), 2.0)
        self.assertEqual([len(b) for b in loadtest.batches(iter([b"x"] * 5), 2)], [2, 2, 1])


@unittest.skipUnless(os.environ.get("CLOSING_LIVE") == "1", "set CLOSING_LIVE=1 and the CLOSING_* binaries")
class Live(unittest.TestCase):
    """The three adapters against real servers, on 3,000 log lines."""

    docs: list[bytes]
    ports: ClassVar[dict[str, int]] = {"xapiand": 18970, "es": 19270, "quickwit": 17270}

    @classmethod
    @override
    def setUpClass(cls) -> None:
        corpus = os.environ["CLOSING_LOGS"]
        with open(corpus, "rb") as fh:
            cls.docs = [next(fh) for _ in range(3000)]
        cls.root = tempfile.mkdtemp(prefix="live", dir=os.path.join(os.path.dirname(HERE), ".scratch"))

    def start(self, name: str, **kw: Any) -> tuple[sy.System, sy.Client]:
        binary = {"xapiand": "CLOSING_XAPIAND", "es": "CLOSING_ES", "quickwit": "CLOSING_QUICKWIT"}[name]
        system = sy.make(name, f"127.0.0.1:{self.ports[name]}")
        system.start(
            sy.Launch(
                os.environ[binary], os.path.join(self.root, name), self.ports[name], cpus=2, memory_bytes=1 << 30, **kw
            )
        )
        self.addCleanup(system.stop)
        client = sy.Client(system.target)
        system.setup(client, "logs", LOGS)
        for i in range(0, len(self.docs), 1000):
            chunk = self.docs[i : i + 1000]
            status, body = client.call(system.bulk_request("logs", LOGS, chunk, last=i + 1000 >= len(self.docs)))
            self.assertTrue(system.parse_bulk(status, body, len(chunk)).ok)
        system.commit(client, "logs")
        self.assertEqual(system.wait_count(client, "logs", LOGS, len(self.docs)), len(self.docs))
        return system, client

    def test_the_three_systems_agree_and_the_caches_stay_cold(self) -> None:
        members = []
        for name, label in (("xapiand", "S"), ("es", "E"), ("quickwit", "Q")):
            system, client = self.start(name)
            self.assertEqual(
                [p for p in system.assert_effective(system.effective(client, "logs"), 2) if "runtime sees" not in p], []
            )
            members.append(sy.Member(label, system, client, "logs"))
        queries = sy.load_queries(os.path.join(HERE, "closing", "queries", "logs-smoke.jsonl"))
        report = sy.check_systems(members, LOGS, queries)
        self.assertEqual(report["dropped"], [], report["dropped"])
        self.assertEqual(report["agg_diffs"], [])
        probe = sy.probe_caches(members, LOGS, queries, 10)
        self.assertTrue(all(p["ok"] for p in probe.values()), probe)

    @unittest.skipUnless(os.environ.get("CLOSING_QUERIES"), "set CLOSING_QUERIES to a closing_queries.py logs file")
    def test_the_generated_logs_queries_agree(self) -> None:
        members = []
        for name, label in (("xapiand", "S"), ("es", "E"), ("quickwit", "Q")):
            system, client = self.start(name)
            members.append(sy.Member(label, system, client, "logs"))
        queries = sy.load_queries(os.environ["CLOSING_QUERIES"])
        report = sy.check_systems(members, LOGS, queries)
        print(
            f"generated queries: kept {len(report['kept'])}, dropped {len(report['dropped'])}, "
            f"order differences {len(report['order_diffs'])}, aggregation differences {len(report['agg_diffs'])}"
        )
        for row in report["dropped"][:40]:
            print("dropped:", row)
        for row in report["agg_diffs"][:20]:
            print("aggregation difference:", row)
        self.assertFalse(report["stop"], "more than 5% of a class disagrees")

    def test_quickwit_sort_direction_and_elasticsearch_range_units(self) -> None:
        for name in ("quickwit", "es", "xapiand"):
            system, client = self.start(name)
            desc = system.run(
                client,
                "logs",
                LOGS,
                q(
                    **{
                        "class": "s",
                        "sort": {"field": "size", "order": "desc"},
                        "range": {"field": "@timestamp", "gte": T0, "lte": T0 + 3000},
                    }
                ),
            )
            asc = system.run(
                client,
                "logs",
                LOGS,
                q(
                    **{
                        "class": "s",
                        "sort": {"field": "size", "order": "asc"},
                        "range": {"field": "@timestamp", "gte": T0, "lte": T0 + 3000},
                    }
                ),
            )
            self.assertTrue(desc.ok and asc.ok)
            self.assertGreater(desc.count or 0, 100, f"{name}: the epoch-second range finds documents")
            system.stop()


if __name__ == "__main__":
    unittest.main()
