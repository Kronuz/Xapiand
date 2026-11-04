#!/usr/bin/env python3
"""closing_rows.py -- the ranked metrics of each gate row, read from a tool's JSON (docs/design/closing-benchmark.md).

  closing_rows.py KIND TOOL.json OUT.rows.json

KIND is the row: t1 and t2 (loadtest, a load), t4 (loadtest --latency, T4a and T4b), t5 (stall_load --saturate), t6
(stall_load --mixed), t7 (loadtest --restart; a crash run's closing_mixed JSON as t7crash), t8 (closing_tenants).
Each metric carries its direction and its axis (query, index, memory, disk; "report" is shown, never ranked against a
margin). closing_report.py reads the rows files.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any

from closing_report import Metric, Rows, write_rows


def lower(value: Any, axis: str = "query") -> Metric | None:
    return Metric(float(value), False, axis) if isinstance(value, (int, float)) and value > 0 else None


def higher(value: Any, axis: str = "index") -> Metric | None:
    return Metric(float(value), True, axis) if isinstance(value, (int, float)) and value > 0 else None


def put(rows: Rows, name: str, metric: Metric | None) -> None:
    if metric is not None:
        rows[name] = metric


def rows_for(kind: str, doc: dict[str, Any]) -> Rows:
    rows: Rows = {}
    if kind in ("t1", "t2"):
        ip = doc.get("index_phase") or {}
        put(rows, f"{kind}.docs_per_sec", higher(ip.get("docs_per_sec")))
        put(rows, f"{kind}.cpu_s_per_1000_docs", lower(ip.get("cpu_seconds_per_1000_docs"), "report"))
        put(rows, f"{kind}.peak_memory_bytes", lower(ip.get("peak_memory_bytes"), "memory"))
        put(rows, f"t3.{kind}.bytes_per_doc", lower(ip.get("bytes_per_doc"), "disk"))
    elif kind == "t4":
        lat = doc.get("latency") or {}
        for name, cls in (lat.get("classes") or {}).items():
            if not cls.get("errors"):
                put(rows, f"t4.{name}.geomean_ms", lower(cls.get("geomean_fastest_ms")))
        put(rows, "t4.server_cpu_us_per_query", lower(lat.get("server_cpu_us_per_query"), "report"))
        put(rows, "t11.idle_memory_current", lower((doc.get("cgroup") or {}).get("memory_current"), "memory"))
    elif kind == "t5":
        for sat in doc.get("saturate") or []:
            put(rows, f"t5.{sat['class']}.rate", higher(sat.get("saturating_rate"), "query"))
            put(rows, f"t5.{sat['class']}.cpu_us_per_query", lower(sat.get("server_cpu_us_per_query"), "report"))
    elif kind == "t6":
        mixed = doc.get("mixed") or {}
        search, write = mixed.get("search") or {}, mixed.get("write") or {}
        for key, node in (
            ("search_p50", (search.get("latency") or {}).get("p50_us")),
            ("search_p99", (search.get("latency") or {}).get("p99_us")),
            ("write_ack_p99", (write.get("write_ack") or {}).get("p99_us")),
            ("visibility_lag_p99", (write.get("visibility_lag") or {}).get("p99_us")),
        ):
            put(rows, f"t6.{key}_us", lower(node))
    elif kind == "t7":
        rec = doc.get("restart") or {}
        put(rows, "t7.clean_start_to_first_answer_s", lower(rec.get("start_to_first_answer_s")))
        put(rows, "t7.clean_first_search_ms", lower(rec.get("first_search_ms")))
    elif kind == "t7crash":
        put(rows, "t7.crash_restart_to_ready_s", lower(doc.get("restart_to_ready_s")))
    elif kind == "t8":
        for shape in doc.get("shapes") or []:
            for r in shape.get("t8_readings") or []:
                if r.get("at_n_min"):
                    put(rows, "t8.anon_per_tenant", lower(r.get("anon_per_tenant"), "memory"))
                    put(rows, "t8.memory_per_tenant", lower(r.get("memory_current_per_tenant"), "memory"))
                    put(rows, "t8.fds_per_tenant", lower(r.get("fds_per_tenant"), "report"))
            cold = shape.get("t9_cold_first_query") or {}
            put(rows, "t9.cold_p50_ms", lower(cold.get("p50_ms")))
            put(rows, "t9.cold_p99_ms", lower(cold.get("p99_ms")))
            wide = shape.get("t10_wide_search") or {}
            put(rows, "t10.wide_p50_ms", lower(wide.get("p50_ms")))
            put(rows, "t10.wide_p99_ms", lower(wide.get("p99_ms")))
            put(rows, "t10.wide_peak_memory_bytes", lower(wide.get("peak_memory_bytes"), "memory"))
    else:
        raise SystemExit(f"unknown row kind {kind}")
    return rows


def main() -> int:
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    kind, src, dst = sys.argv[1:]
    rows = rows_for(kind, json.loads(Path(src).read_text()))
    write_rows(Path(dst), rows)
    print(f"{dst}: {len(rows)} metrics")
    return 0


if __name__ == "__main__":
    sys.exit(main())
