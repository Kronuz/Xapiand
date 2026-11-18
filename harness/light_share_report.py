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
"""light_share_report.py FILE...: one row per stall_load run (ls-<i>-<arm>.json), then each arm's medians.

T5's limit is 2x the unloaded light query p99 + 1 ms; T4's is every HTTP loop's loaded p99 lag at most 1,024 us.
"""

from __future__ import annotations

import json
import os
import re
import statistics
import sys
from typing import Any

COLS = (
    "unl_p99",
    "t5_limit",
    "query_p99",
    "search_p99",
    "get_p99",
    "pair_p99",
    "server_p99",
    "lag_p99",
    "heavy",
    "heavy_p99",
    "restored",
)


def get(o: Any, path: str) -> Any:
    for p in path.split("."):
        o = o.get(p) if isinstance(o, dict) else None
    return o


def row(path: str) -> dict[str, float]:
    d = json.load(open(path))
    unl, ld = d["phases"][0], d["phases"][-1]
    unl_p99 = float(get(unl, "light.query.p99_us") or 0) / 1000
    lags = {k: float(v["p99_us"]) for k, v in (ld.get("lag") or {}).items() if k.startswith("http")}
    return {
        "unl_p99": unl_p99,
        "t5_limit": 2 * unl_p99 + 1,
        "query_p99": float(get(ld, "light.query.p99_us") or 0) / 1000,
        "search_p99": float(get(ld, "light.search.p99_us") or 0) / 1000,
        "get_p99": float(get(ld, "light.get.p99_us") or 0) / 1000,
        "pair_p99": float(get(ld, "light.pair.p99_us") or 0) / 1000,
        "server_p99": float(get(ld, "light.probe.all.parts.server.p99_us") or 0) / 1000,
        "lag_p99": max(lags.values()) / 1000 if lags else 0.0,
        "heavy": float(get(ld, "heavy.searches") or 0),
        "heavy_p99": max((float(v["p99_us"]) for v in (get(ld, "heavy.latency") or {}).values()), default=0.0) / 1000,
        "restored": float(get(ld, "heavy.restored_docs") or 0),
    }


def main() -> int:
    arms: dict[str, list[dict[str, float]]] = {}
    print("run".ljust(14) + "".join(c.rjust(11) for c in COLS))
    for path in sorted(
        sys.argv[1:], key=lambda p: int(m[1]) if (m := re.search(r"-(\d+)-", os.path.basename(p))) else 0
    ):
        m = re.search(r"-(\d+)-([^-]+)\.json$", os.path.basename(path))
        if not m:
            continue
        r = row(path)
        arms.setdefault(m[2], []).append(r)
        print(
            f"{m[1]}-{m[2]}".ljust(14)
            + "".join(f"{r[c]:11.2f}" if c != "heavy" and c != "restored" else f"{r[c]:11.0f}" for c in COLS)
        )
    print("median by arm (ms; heavy searches and restored docs per loaded phase; heavy_p99 is the worst heavy kind's)")
    for arm, rs in arms.items():
        print(arm.ljust(14) + "".join(f"{statistics.median(r[c] for r in rs):11.2f}" for c in COLS))
    return 0


if __name__ == "__main__":
    sys.exit(main())
