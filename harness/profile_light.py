#!/usr/bin/env python3
"""profile_light.py -- Phase 7c K6 / T11: what one light request costs in dispatch, CPU and wall.

Starts a solo strata node from BIN, loads --docs accounts, then drives one connection (concurrency 1)
of GET-by-id and single-term searches for --secs while `sample` records every thread at 1 ms.

  CPU   Each sample is 1 ms of one thread. Busy samples are put in a named group by their stack:
        the offload hop (posting to the pool, waking and dequeueing a worker, posting back), the
        scheduler (task and WorkPool frames; none before 7c), the engine, and HTTP. CPU per request
        is the group's samples over the requests the client completed.
  wall  From BIN's HTTP_TRACE_HOP build (http branch trace/hop): the stamps at the post, at the start
        of the run on the pool thread, at the end of the run, and at the resume on the loop. The hop's
        wall is (start - post) + (resume - end); with no trace file the wall columns are null.

A binary built after 7c has no hop: its dispatch is the scheduler group, and the same script reads it.
The dry run on a pre-7c build must show a non-zero hop share in both columns.

  profile_light.sh BIN OUT [--secs 10] [--docs 20000] [--label NAME] [--port 18991]
  profile_light.sh --compare PRE.json POST.json       T11: dispatch cost, post over pre, in CPU and in wall

Run it under a hold: quiet strict NAME MIN "what" -- harness/profile_light.sh BIN OUT
"""

from __future__ import annotations

import argparse
import http.client
import json
import os
import random
import re
import shutil
import signal
import socket
import statistics
import subprocess
import sys
import time
from typing import Any

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATASET = os.path.join(REPO, "docs", "public", "assets", "accounts.ndjson")
GROUPS = ("hop", "scheduler", "engine", "http", "other")

IDLE = re.compile(
    r"psynch_cvwait|^kevent|mach_msg|semwait|__select|nanosleep|semaphore_wait|^poll|__sigwait|psynch_mutexwait|"
    r"ulock_wait|futex_wait|_pthread_cond_wait|do_timed_wait|gettimeofday|mach_timebase_info|clock_gettime"
)
# A frame decides the group when it is the nearest to the leaf that matches one of these (leaf to root).
ENGINE = re.compile(
    r"engine::|\bstrata::|\bprospect::|DatabaseHandler|Shard::|Schema::|MsgPack|msgpack::|lock_shard|DatabasePool|ShardEndpoint"
)
HOP = re.compile(
    r"thread_pool|psynch_cvsignal|cond_signal|cond_broadcast|wake_one|post_immediate|post_deferred|asio::post|"
    r"asio::dispatch|work_dispatcher|signal_blocker|trace_hop|posix_event|\bco_spawn\b|co_spawn_async"
)
SCHED = re.compile(r"\btask::|WorkPool|threadpool::|WorkGroup|budget")
HTTP = re.compile(
    r"http::|\bHttp|llhttp|http_parser|AsioResponseWriter|search_views|SearchService|query_dsl|rapidjson|Request::|Response|"
    r"kqueue_reactor|epoll_reactor|reactive_socket|descriptor_ops|socket_ops|Reactor::run"
)
POOL = re.compile(r"asio::thread_pool|thread_pool::thread_function")
SAMPLE_LINE = re.compile(r"^([ +!:|]*?)(\d+) (.*)$")


def log(msg: str) -> None:
    print(f"[profile_light {time.strftime('%H:%M:%S')}] {msg}", file=sys.stderr, flush=True)


def clean(name: str) -> str:
    name = re.sub(r"\s+\(in ([^)]*)\).*$", lambda m: " @" + m.group(1) if "xapiand" not in m.group(1) else "", name)
    name = re.sub(r"\s+\+\s+[\d,.]+\s+\[0x.*$", "", name)
    return name[:140]


def parse_sample(path: str) -> tuple[list[tuple[str, list[str], int]], int]:
    """Self-time rows (thread, frames root..leaf, samples) of `sample`'s call graph, and the busiest thread's total."""
    nodes: list[list[Any]] = []
    stack: list[list[Any]] = []
    inside = False
    with open(path, errors="replace") as fh:
        for raw in fh:
            if raw.startswith("Call graph:"):
                inside = True
                continue
            if not inside:
                continue
            if raw.startswith(("Total number in stack", "Sort by top")):
                break
            m = SAMPLE_LINE.match(raw.rstrip("\n"))
            if not m:
                continue
            depth, count = len(m.group(1)), int(m.group(2))
            while stack and stack[-1][0] >= depth:
                stack.pop()
            node: list[Any] = [depth, count, m.group(3), 0, [x[2] for x in stack]]
            if stack:
                stack[-1][3] += count
            stack.append(node)
            nodes.append(node)
    rows: list[tuple[str, list[str], int]] = []
    for _depth, count, name, child_total, ancestors in nodes:
        if count - child_total > 0 and ancestors:
            rows.append((ancestors[0], [clean(x) for x in ancestors[1:]] + [clean(name)], count - child_total))
    return rows, max((n[1] for n in nodes if not n[4]), default=0)


def classify(frames: list[str]) -> str | None:
    """The group of a busy stack, or None for an idle one; the frame nearest the leaf that names a group decides."""
    if IDLE.search(frames[-1]):
        return None
    for frame in reversed(frames):
        if ENGINE.search(frame):
            return "engine"
        if HOP.search(frame):
            return "hop"
        if SCHED.search(frame):
            return "scheduler"
        if HTTP.search(frame):
            return "http"
    # Nothing names a group: asio plumbing on a pool worker is the dequeue side of the hop.
    return "hop" if any(POOL.search(f) for f in frames) else "other"


def cpu_groups(rows: list[tuple[str, list[str], int]]) -> dict[str, int]:
    out = dict.fromkeys(GROUPS, 0)
    for _thread, frames, samples in rows:
        group = classify(frames)
        if group is not None:
            out[group] += samples
    return out


def wall_hop(path: str, tail: int) -> dict[str, Any] | None:
    if not os.path.exists(path):
        return None
    stamps: list[tuple[int, int, int, int]] = []
    with open(path) as fh:
        for line in fh:
            parts = line.strip().split(",")
            if len(parts) == 4 and all(p.isdigit() for p in parts):
                post, start, end, resume = (int(p) for p in parts)
                if post <= start <= end <= resume:
                    stamps.append((post, start, end, resume))
    stamps = stamps[-tail:] if tail > 0 else []
    if not stamps:
        return None
    out_us = [(s[1] - s[0]) / 1e3 for s in stamps]
    back_us = [(s[3] - s[2]) / 1e3 for s in stamps]
    run_us = [(s[2] - s[1]) / 1e3 for s in stamps]
    hop_us = [a + b for a, b in zip(out_us, back_us, strict=True)]
    total_us = [(s[3] - s[0]) / 1e3 for s in stamps]
    return {
        "requests": len(stamps),
        "hop_wall_us_median": statistics.median(hop_us),
        "hop_wall_us_mean": statistics.fmean(hop_us),
        "post_to_start_us_median": statistics.median(out_us),
        "end_to_resume_us_median": statistics.median(back_us),
        "handler_run_us_median": statistics.median(run_us),
        "hop_share_of_offloaded_wall": sum(hop_us) / sum(total_us),
    }


def run(args: argparse.Namespace) -> dict[str, Any]:
    binary = os.path.abspath(args.bin)
    out_dir = os.path.abspath(args.out)
    os.makedirs(out_dir, exist_ok=True)
    work = os.path.join(out_dir, f"{args.label}.work")
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(os.path.join(work, "data"))
    trace = os.path.join(work, "hop.csv")
    raw = os.path.join(out_dir, f"{args.label}.sample.txt")
    env = dict(os.environ, HTTP_TRACE_HOP_OUT=trace, XAPIAND_LAG_PROBE="0")
    with socket.socket() as probe:
        probe.settimeout(1)
        if probe.connect_ex(("127.0.0.1", args.port)) == 0:
            raise SystemExit(f"127.0.0.1:{args.port} is already in use; pick another --port")
    logf = open(os.path.join(work, "node.log"), "wb")
    proc = subprocess.Popen(
        [binary, "--port", str(args.port), "--name", "light", "-D", os.path.join(work, "data")],
        stdout=logf,
        stderr=logf,
        env=env,
        start_new_session=True,
    )
    sampler: subprocess.Popen[bytes] | None = None
    try:
        for _ in range(240):
            if proc.poll() is not None:
                raise SystemExit(f"the node exited with {proc.returncode}; see {work}/node.log")
            try:
                c = http.client.HTTPConnection("127.0.0.1", args.port, timeout=3)
                c.request("GET", "/")
                c.getresponse().read()
                break
            except OSError:
                time.sleep(0.5)
        else:
            raise SystemExit("the node did not come up")

        with open(DATASET, "rb") as fh:
            lines = [ln for ln in fh.read().split(b"\n") if ln.strip()]
        states = sorted({json.loads(ln)["contact"]["state"] for ln in lines})
        conn = http.client.HTTPConnection("127.0.0.1", args.port, timeout=120)
        body = b"\n".join(lines[i % len(lines)][:-1] + b',"_id":%d}' % (i + 1) for i in range(args.docs)) + b"\n"
        conn.request("RESTORE", "/accounts/?commit", body=body, headers={"Content-Type": "application/x-ndjson"})
        resp = conn.getresponse()
        resp.read()
        if resp.status >= 300:
            raise SystemExit(f"loading accounts: HTTP {resp.status}")
        rng = random.Random(1)

        def one() -> float:
            t = time.perf_counter()
            if rng.random() < 0.5:
                conn.request("GET", f"/accounts/{rng.randint(1, args.docs)}")
            else:
                q = json.dumps({"_query": {"contact.state": rng.choice(states)}, "_limit": 10}).encode()
                conn.request("SEARCH", "/accounts/", body=q, headers={"Content-Type": "application/json"})
            r = conn.getresponse()
            r.read()
            if r.status >= 300:
                raise SystemExit(f"request failed: HTTP {r.status}")
            return time.perf_counter() - t

        warm_end = time.perf_counter() + args.warmup
        warm = 0
        while time.perf_counter() < warm_end:
            one()
            warm += 1
        sampler = subprocess.Popen(
            ["sample", str(proc.pid), str(args.secs), "-file", raw],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        time.sleep(1.5)  # `sample` suspends the node while it attaches and loads symbols
        # ab, one keep-alive connection: a C client, so the server sets the pace and the samples see its work.
        t0 = time.perf_counter()
        ab = subprocess.run(
            ["ab", "-q", "-k", "-c", "1", "-n", "100000000", "-t", str(args.secs - 3),
             f"http://127.0.0.1:{args.port}/accounts/{args.docs // 2}"],
            capture_output=True, text=True, check=False,
        )  # fmt: skip
        wall = time.perf_counter() - t0
        done = re.search(r"Complete requests:\s+(\d+)", ab.stdout)
        failed = re.search(r"Failed requests:\s+(\d+)", ab.stdout)
        mean_ms = re.search(r"Time per request:\s+([\d.]+) \[ms\] \(mean\)", ab.stdout)
        if not done or int(done.group(1)) == 0 or (failed and int(failed.group(1)) > 0) or not mean_ms:
            raise SystemExit(f"ab failed: {ab.stdout[-400:]} {ab.stderr[-200:]}")
        sampler.wait()
        sampler = None
    finally:
        if sampler is not None:
            sampler.kill()
        # SIGTERM through exit() lets the hop log write its file.
        if proc.poll() is None:
            os.killpg(proc.pid, signal.SIGTERM)
            try:
                proc.wait(timeout=30)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid, signal.SIGKILL)
        logf.close()

    rows, per_thread = parse_sample(raw)
    groups = cpu_groups(rows)
    n = int(done.group(1))
    busy = sum(groups.values())
    # `sample` takes about one sample per thread every few ms, not every ms: a sample stands for the window over the
    # samples one thread got, and a thread caught busy in k of them used k such intervals of CPU.
    ms_per_sample = args.secs * 1000.0 / per_thread
    cpu_ms_per_request = {g: groups[g] * ms_per_sample / n for g in GROUPS}
    # The measured requests are the last n offloaded ones in the trace: nothing is served after the client stops.
    wall_stats = wall_hop(trace, tail=n)
    result: dict[str, Any] = {
        "label": args.label,
        "bin": binary,
        "commit": subprocess.run(
            ["git", "-C", REPO, "rev-parse", "--short", "HEAD"], capture_output=True, text=True, check=False
        ).stdout.strip(),
        "docs": args.docs,
        "client_requests": n,
        "client_wall_s": round(wall, 3),
        "latency_us": {
            "mean": float(mean_ms.group(1)) * 1e3,
        },
        "ms_per_sample": round(ms_per_sample, 3),
        "cpu_samples": groups,
        "cpu_ms_per_request": {g: round(v, 5) for g, v in cpu_ms_per_request.items()},
        "cpu_ms_per_request_total": round(busy * ms_per_sample / n, 5),
        "hop_cpu_share": round(groups["hop"] / busy, 4) if busy else None,
        "dispatch_cpu_ms_per_request": round((groups["hop"] + groups["scheduler"]) * ms_per_sample / n, 5),
        "wall": wall_stats,
        "dispatch_wall_us_median": wall_stats["hop_wall_us_median"] if wall_stats else None,
        "hop_wall_share": wall_stats["hop_share_of_offloaded_wall"] if wall_stats else None,
    }
    with open(os.path.join(out_dir, f"{args.label}.json"), "w") as fh:
        json.dump(result, fh, indent=1)
    shutil.rmtree(work, ignore_errors=True)
    log(
        f"{args.label}: {n} requests, mean {result['latency_us']['mean']:.0f} us; CPU ms/request "
        + ", ".join(f"{g} {cpu_ms_per_request[g]:.4f}" for g in GROUPS)
        + f"; hop CPU share {result['hop_cpu_share']}, hop wall share {result['hop_wall_share']}"
    )
    return result


def compare(pre_path: str, post_path: str) -> int:
    with open(pre_path) as fh:
        pre = json.load(fh)
    with open(post_path) as fh:
        post = json.load(fh)
    rows = []
    cpu_ratio = (
        post["dispatch_cpu_ms_per_request"] / pre["dispatch_cpu_ms_per_request"]
        if pre["dispatch_cpu_ms_per_request"]
        else None
    )
    wall_ratio = None
    if pre.get("dispatch_wall_us_median") and post.get("dispatch_wall_us_median") is not None:
        wall_ratio = post["dispatch_wall_us_median"] / pre["dispatch_wall_us_median"]
    elif pre.get("dispatch_wall_us_median"):
        wall_ratio = 0.0
    rows.append(
        ("dispatch CPU ms/request", pre["dispatch_cpu_ms_per_request"], post["dispatch_cpu_ms_per_request"], cpu_ratio)
    )
    rows.append(
        (
            "dispatch wall us (median)",
            pre.get("dispatch_wall_us_median"),
            post.get("dispatch_wall_us_median"),
            wall_ratio,
        )
    )
    print(f"{'row':28} {'pre-7c':>12} {'post-7c':>12} {'post/pre':>10}")
    for name, a, b, r in rows:
        print(
            f"{name:28} {a if a is not None else '-':>12} {b if b is not None else '-':>12} {(f'{r:.3f}') if r is not None else '-':>10}"
        )
    ok = cpu_ratio is not None and cpu_ratio <= 0.5 and wall_ratio is not None and wall_ratio <= 0.5
    print("T11 (dispatch at most half, in CPU and in wall):", "met" if ok else "not met")
    return 0 if ok else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("bin", nargs="?")
    ap.add_argument("out", nargs="?")
    ap.add_argument("--secs", type=int, default=10)
    ap.add_argument("--docs", type=int, default=20000)
    ap.add_argument("--warmup", type=float, default=3.0)
    ap.add_argument("--label", default="light")
    ap.add_argument("--port", type=int, default=18991)
    ap.add_argument("--compare", nargs=2, metavar=("PRE", "POST"))
    args = ap.parse_args()
    if args.compare:
        return compare(*args.compare)
    if not args.bin or not args.out:
        ap.error("give BIN and OUT, or --compare PRE POST")
    result = run(args)
    ok = result["cpu_samples"]["hop"] > 0
    if not ok:
        log("no sample landed in the hop group: a pre-7c build must show one")
    return 0


if __name__ == "__main__":
    sys.exit(main())
