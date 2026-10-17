#!/usr/bin/env python3
"""tls_cost.py BINARY [options]

T4 (docs/design/transport-security.md): the server's CPU seconds per 1,000 requests, TLS
against plain HTTP, for three workloads, each on a fresh solo node:
  search      a keep-alive SEARCH loop (loadtest.py's index and query phases)
  e2e         the docs E2E collection (newman 6), the docs' own request mix
  handshakes  a new connection per request; over TLS each is a full handshake (no
              resumption), so its requests per CPU second are handshakes per second at one core
Each trial runs both schemes back to back per workload, alternating which goes first. The
server's CPU seconds are read before and after the measured phase only, from /proc/<pid>/stat
on Linux and `ps -o cputime` on macOS. The client is this process (and newman); on Linux
--server-cpus and --client-cpus pin them apart. macOS cannot pin, and says so.

Prints each trial, the median, the spread ((max - min) / median), and the TLS/plain ratio
of the medians and per trial; --out writes the JSON. The real measurement belongs on a
quiet host: run one short trial elsewhere only to check the script works.

  python3 -I harness/tls_cost.py build/bin/xapiand --trials 5 --duration 30 --out cost.json
  python3 -I harness/tls_cost.py build/bin/xapiand --trials 1 --duration 2 --replicate 1   # a check
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import statistics
import subprocess
import sys
import threading
import time
import urllib.request
from collections.abc import Callable
from typing import Any

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
sys.path.insert(0, HERE)  # -I drops the script's directory; loadtest is a sibling
import loadtest


def cpu_seconds(pid: int) -> float:
    """The process's user plus system CPU seconds."""
    if sys.platform.startswith("linux"):
        with open(f"/proc/{pid}/stat") as f:
            fields = f.read().rsplit(")", 1)[1].split()
        return (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")  # utime, stime
    out = subprocess.run(["ps", "-o", "cputime=", "-p", str(pid)], capture_output=True, text=True, check=True)
    text = out.stdout.strip()  # [[dd-]hh:]mm:ss.cc
    days = 0
    if "-" in text:
        d, text = text.split("-", 1)
        days = int(d)
    seconds = 0.0
    for part in text.split(":"):
        seconds = seconds * 60 + float(part)
    return days * 86400 + seconds


def parse_cpus(spec: str | None) -> set[int] | None:
    if not spec:
        return None
    cpus: set[int] = set()
    for item in spec.split(","):
        lo, _, hi = item.partition("-")
        cpus.update(range(int(lo), int(hi or lo) + 1))
    return cpus


class Node:
    def __init__(self, args: argparse.Namespace, scheme: str, name: str) -> None:
        self.scheme = scheme
        self.data = os.path.join(args.work, name)
        shutil.rmtree(self.data, ignore_errors=True)
        os.makedirs(self.data)
        flags = []
        if scheme == "https":
            flags = [
                "--tls-cert",
                os.path.join(args.work, "certs", "server.pem"),
                "--tls-key",
                os.path.join(args.work, "certs", "server.key"),
            ]
        server_cpus = parse_cpus(args.server_cpus)

        def pin() -> None:
            if server_cpus:
                os.sched_setaffinity(0, server_cpus)

        self.log = open(self.data + ".log", "w")  # noqa: SIM115 -- open for the node's life
        self.proc = subprocess.Popen(
            [args.bin, "--port", str(args.port), "--name", "tlscost", "--solo", "-D", self.data, *flags],
            stdout=self.log,
            stderr=subprocess.STDOUT,
            preexec_fn=pin if server_cpus else None,  # noqa: PLW1509 -- no client threads run yet
        )
        self.url = f"{scheme}://localhost:{args.port}"
        loadtest.use_ca(os.path.join(args.work, "certs", "ca.pem") if scheme == "https" else None)
        context = loadtest.TLS_CONTEXT  # loadtest's connections now speak this node's scheme
        for _ in range(120):
            if self.proc.poll() is not None:
                raise SystemExit(f"node exited; see {self.log.name}")
            try:
                with urllib.request.urlopen(self.url + "/", context=context, timeout=2) as r:
                    if r.status == 200:
                        return
            except Exception:  # noqa: BLE001, S110 -- not ready yet
                pass
            time.sleep(0.5)
        raise SystemExit(f"node not ready; see {self.log.name}")

    def cpu(self) -> float:
        return cpu_seconds(self.proc.pid)

    def stop(self) -> None:
        self.proc.terminate()
        try:
            self.proc.wait(30)
        except subprocess.TimeoutExpired:
            self.proc.kill()
        self.log.close()


def measured(node: Node, phase: Callable[[], tuple[int, int]]) -> dict[str, Any]:
    """Runs phase() between two reads of the server's CPU; phase returns (requests, errors)."""
    before, start = node.cpu(), time.monotonic()
    requests, errors = phase()
    after, wall = node.cpu(), time.monotonic() - start
    cpu = after - before
    return {
        "requests": requests,
        "errors": errors,
        "cpu_seconds": round(cpu, 3),
        "wall_seconds": round(wall, 2),
        "cpu_per_1000": round(cpu * 1000 / requests, 4) if requests else None,
        "requests_per_cpu_second": round(requests / cpu, 1) if cpu > 0 else None,
    }


def search(args: argparse.Namespace, node: Node) -> dict[str, Any]:
    target = f"localhost:{args.port}"
    index = loadtest.phase_index(target, "cost", args.dataset, args.replicate, None)
    if not index["ok"]:
        raise SystemExit(f"RESTORE failed: {index['restore_status']}")

    def phase() -> tuple[int, int]:
        q = loadtest.phase_query(target, "cost", args.concurrency, args.duration)
        return q["total"] + q["errors"], q["errors"]

    return measured(node, phase)


def e2e(args: argparse.Namespace, node: Node) -> dict[str, Any]:
    collection = os.path.join(args.work, f"collection-{node.scheme}.json")
    with open(collection, "w") as out:
        subprocess.run(
            [sys.executable, os.path.join(REPO, "docs_to_postman.py")],
            stdout=out,
            check=True,
            env={**os.environ, "XAPIAND_URL": node.url},
            cwd=REPO,
        )
    report = os.path.join(args.work, f"newman-{node.scheme}.json")
    ca = ["--ssl-extra-ca-certs", os.path.join(args.work, "certs", "ca.pem")] if node.scheme == "https" else []

    def phase() -> tuple[int, int]:
        subprocess.run(
            [
                args.newman,
                "run",
                collection,
                *ca,
                "--reporters",
                "json",
                "--reporter-json-export",
                report,
                "--timeout-request",
                "30000",
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            cwd=REPO,
            check=False,
        )  # failures are counted from the report
        with open(report) as f:
            stats = json.load(f)["run"]["stats"]
        return stats["requests"]["total"], stats["requests"]["failed"] + stats["assertions"]["failed"]

    return measured(node, phase)


def handshakes(args: argparse.Namespace, node: Node) -> dict[str, Any]:
    target = f"localhost:{args.port}"

    def phase() -> tuple[int, int]:
        stop_at = time.monotonic() + args.duration
        counts = [0, 0]
        lock = threading.Lock()

        def worker() -> None:
            done = failed = 0
            while time.monotonic() < stop_at:
                conn = loadtest.connect(target)
                try:
                    status, _ = loadtest.request(conn, "GET", "/", headers={"Connection": "close"})
                    done += 1
                    failed += status != 200
                except Exception:  # noqa: BLE001 -- any failure counts
                    failed += 1
                finally:
                    conn.close()
            with lock:
                counts[0] += done
                counts[1] += failed

        threads = [threading.Thread(target=worker) for _ in range(args.concurrency)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        return counts[0], counts[1]

    return measured(node, phase)


WORKLOADS = {"search": search, "e2e": e2e, "handshakes": handshakes}


def summary(runs: list[dict[str, Any]], key: str) -> dict[str, Any]:
    values = [r[key] for r in runs if r[key] is not None]
    if not values:
        return {"values": []}
    median = statistics.median(values)
    return {
        "values": values,
        "median": median,
        "spread": round((max(values) - min(values)) / median, 4) if median else None,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("bin")
    ap.add_argument("--port", type=int, default=18897)
    ap.add_argument("--trials", type=int, default=5)
    ap.add_argument("--duration", type=float, default=30, help="seconds per search and handshakes run")
    ap.add_argument("--concurrency", type=int, default=8)
    ap.add_argument("--replicate", type=int, default=5, help="copies of the accounts dataset to index")
    ap.add_argument("--dataset", default=os.path.join(REPO, "docs", "public", "assets", "accounts.ndjson"))
    ap.add_argument("--workloads", default="search,e2e,handshakes")
    ap.add_argument("--newman", default=shutil.which("newman") or "newman")
    ap.add_argument("--server-cpus", help="Linux: CPUs for the server, such as 0-3")
    ap.add_argument("--client-cpus", help="Linux: CPUs for this client and newman, such as 4-7")
    ap.add_argument("--work", default=os.path.join(REPO, ".scratch", "tls_cost"))
    ap.add_argument("--out")
    args = ap.parse_args()

    workloads = [w for w in args.workloads.split(",") if w]
    for w in workloads:
        if w not in WORKLOADS:
            ap.error(f"unknown workload {w} (valid: {', '.join(WORKLOADS)})")
    if "e2e" in workloads:
        version = subprocess.run([args.newman, "--version"], capture_output=True, text=True, check=False).stdout.strip()
        if int(version.split(".")[0] or 0) < 6:
            ap.error(f"the e2e workload needs newman 6 or later (found {version or 'none'})")
    client_cpus = parse_cpus(args.client_cpus)
    if (client_cpus or args.server_cpus) and not hasattr(os, "sched_setaffinity"):
        ap.error("--server-cpus and --client-cpus need Linux")
    if client_cpus:
        os.sched_setaffinity(0, client_cpus)
    pinning = (
        f"server {args.server_cpus}, client {args.client_cpus}"
        if args.server_cpus or client_cpus
        else ("none (macOS cannot pin)" if sys.platform == "darwin" else "none")
    )

    shutil.rmtree(args.work, ignore_errors=True)
    os.makedirs(args.work)
    subprocess.run(
        [os.path.join(HERE, "tls_certs.sh"), os.path.join(args.work, "certs")], check=True, stdout=subprocess.DEVNULL
    )
    print(
        f"== [T4] TLS cost: {args.trials} trials of {', '.join(workloads)}; CPU from "
        f"{'/proc/<pid>/stat' if sys.platform.startswith('linux') else 'ps -o cputime'}; pinning {pinning}",
        flush=True,
    )

    runs: dict[str, dict[str, list[dict[str, Any]]]] = {w: {"https": [], "http": []} for w in workloads}
    for trial in range(args.trials):
        for w in workloads:
            order = ["https", "http"] if trial % 2 == 0 else ["http", "https"]
            for scheme in order:
                node = Node(args, scheme, f"{w}-{scheme}-{trial}")
                try:
                    r = WORKLOADS[w](args, node)
                finally:
                    node.stop()
                runs[w][scheme].append(r)
                print(
                    f"  trial {trial + 1} {w:10} {scheme:5}: {r['requests']} requests ({r['errors']} errors), "
                    f"{r['cpu_seconds']} CPU s, {r['cpu_per_1000']} CPU s per 1,000, "
                    f"{r['requests_per_cpu_second']} per CPU second",
                    flush=True,
                )

    result: dict[str, Any] = {
        "bin": args.bin,
        "platform": sys.platform,
        "pinning": pinning,
        "trials": args.trials,
        "duration": args.duration,
        "concurrency": args.concurrency,
        "workloads": {},
    }
    for w in workloads:
        tls, plain = summary(runs[w]["https"], "cpu_per_1000"), summary(runs[w]["http"], "cpu_per_1000")
        paired = [
            round(a["cpu_per_1000"] / b["cpu_per_1000"], 4)
            for a, b in zip(runs[w]["https"], runs[w]["http"])
            if a["cpu_per_1000"] and b["cpu_per_1000"]
        ]
        ratio = round(tls["median"] / plain["median"], 4) if tls.get("median") and plain.get("median") else None
        rate = summary(runs[w]["https"], "requests_per_cpu_second")
        result["workloads"][w] = {
            "https": runs[w]["https"],
            "http": runs[w]["http"],
            "cpu_per_1000_https": tls,
            "cpu_per_1000_http": plain,
            "ratio_of_medians": ratio,
            "paired_ratios": paired,
            "https_requests_per_cpu_second": rate,
        }
        print(
            f"{w}: CPU s per 1,000, TLS median {tls.get('median')} (spread {tls.get('spread')}), "
            f"plain median {plain.get('median')} (spread {plain.get('spread')}); TLS/plain {ratio}, "
            f"per trial {paired}"
            + (f"; TLS handshakes per CPU second {rate.get('median')}" if w == "handshakes" else ""),
            flush=True,
        )
    if args.out:
        with open(args.out, "w") as f:
            json.dump(result, f, indent=2)
        print(f"wrote {args.out}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
