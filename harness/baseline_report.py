#!/usr/bin/env python3
"""baseline_report.py OUTDIR -- the tables of docs/perf/phase7c-baseline.md, from baseline_7c.sh's result files."""

from __future__ import annotations

import json
import os
import sys
from typing import Any


def load(path: str) -> dict[str, Any] | None:
    if not os.path.exists(path):
        return None
    with open(path) as fh:
        data: dict[str, Any] = json.load(fh)
    return data


def ms(us: float | None) -> str:
    return "-" if us is None else f"{us / 1000.0:,.1f}"


def main(out: str) -> int:
    stall = load(os.path.join(out, "stall_suite.json"))
    light = load(os.path.join(out, "light", "pre7c.json"))
    loadtest = load(os.path.join(out, "loadtest.json"))
    bulk = load(os.path.join(out, "bulk", "restore20k.json"))
    if stall:
        meta = stall["meta"]
        print(
            f"stall_load: commit {meta['commit']}, {meta['host_cpus']} CPUs, {meta['reactors']} reactors, "
            f"accounts {meta['accounts_docs']:,} documents (loaded in {meta['load_accounts_s']} s), "
            f"{meta['duration']:g} s phases, {meta['rate_note']}, total {meta['total_s']} s\n"
        )
        sat = stall.get("saturate")
        if sat:
            print(
                f"saturating light rate: {sat['saturating_rate']:.0f}/s (first bad {sat['first_bad_rate']:.0f}/s; "
                f"unloaded p99 {ms(sat['unloaded_p99_us'])} ms (the lower of the first two steps, floor 1 ms), limit {ms(sat['limit_p99_us'])} ms)\n"
            )
            print("| rate /s | query p99 ms | good |\n|---:|---:|:--|")
            for step in sat["trace"]:
                print(f"| {step['rate']:g} | {ms(step['p99_us'])} | {'yes' if step['good'] else 'no'} |")
            print()
        print(
            "| phase | light /s (target, issued) | query p50 ms | p99 ms | p99.9 ms | unfinished | 503 | other err | "
            "worst loop p99 / p99.9 lag (us) | accounts segments (backlog) | RSS MB |"
        )
        print("|---|---|---:|---:|---:|---:|---:|---:|---|---|---:|")
        for p in stall["phases"]:
            if p["name"].startswith("saturate"):
                continue
            li = p.get("light")
            if not li:
                continue
            q = li["query"]
            lag = p.get("lag") or {}
            worst = max(lag.items(), key=lambda kv: kv[1]["p99_us"] or 0, default=None)
            lagtxt = "-" if worst is None else f"{worst[1]['p99_us']:.0f} / {worst[1]['p999_us']:.0f} ({worst[0]})"
            acc = p["index"]["accounts"]
            seg = "-" if acc["segments"] is None else f"{acc['segments']} ({acc['merge_backlog']})"
            rss = "-" if p.get("rss_kb") is None else f"{p['rss_kb'] / 1024:,.0f}"
            print(
                f"| {p['name']} | {li['target_rate']:g}, {li['achieved_rate']:g} | {ms(q['p50_us'])} | {ms(q['p99_us'])} | "
                f"{ms(q['p999_us'])} | {li.get('unfinished_at_drain_cutoff', 0)} | {li['errors']['http_503']} | "
                f"{li['errors']['http_other']} | {lagtxt} | {seg} | {rss} |"
            )
        print()
        for p in stall["phases"]:
            h = p.get("heavy")
            if h and not p["name"].startswith("saturate"):
                print(
                    f"- {p['name']}: heavy {h['searches']} searches, {h['restored_docs']:,} documents restored into "
                    f"{p['restore_index']}, {h['errors']['http_503']} 503"
                )
        print()
        loaded = next((p for p in stall["phases"] if p["name"] == "loaded"), None)
        if loaded and loaded.get("lag"):
            print("per-loop lag in the loaded phase (bucket upper edges, us):\n")
            print("| loop | wakes | p50 | p99 | p99.9 | max so far |\n|---|---:|---:|---:|---:|---:|")
            for name, v in loaded["lag"].items():
                print(
                    f"| {name} | {v['wakes']} | {v['p50_us']:.0f} | {v['p99_us']:.0f} | {v['p999_us']:.0f} | {v['max_us_so_far']:.0f} |"
                )
            print()
        if loaded and loaded.get("census"):
            c = loaded["census"]
            print(
                "thread census mid-loaded-phase: "
                + ", ".join(f"{k} {v}" for k, v in c["threads"].items())
                + f"; total {c['total']}\n"
            )
    if light:
        w = light["wall"] or {}
        print(
            f"profile_light (T11, concurrency 1, GET by id, {light['client_requests']:,} requests, mean {light['latency_us']['mean']:.0f} us): "
            f"CPU ms/request "
            + ", ".join(f"{g} {v:.4f}" for g, v in light["cpu_ms_per_request"].items())
            + f"; hop CPU share {light['hop_cpu_share']}; hop wall median {w.get('hop_wall_us_median', 0):.1f} us "
            f"(post to start {w.get('post_to_start_us_median', 0):.1f}, end to resume {w.get('end_to_resume_us_median', 0):.1f}), "
            f"handler run median {w.get('handler_run_us_median', 0):.1f} us, hop share of offloaded wall {w.get('hop_share_of_offloaded_wall', 0):.4f}\n"
        )
    if loadtest:
        ip, qp = loadtest["index_phase"], loadtest["query_phase"]
        print(
            f"loadtest (T12): RESTORE {ip['docs']:,} documents {ip['docs_per_sec']:,} docs/s (median of "
            f"{', '.join(str(t['docs_per_sec']) for t in loadtest['index_trials'])}); queries at 16 clients {qp['qps']:,} QPS "
            f"(median of {', '.join(str(t['qps']) for t in loadtest['query_trials'])}), p50 {qp['latency_ms']['p50']} ms, p99 {qp['latency_ms']['p99']} ms\n"
        )
    if bulk:
        s = bulk["smoke"]
        print(
            f"profile_bulk smoke (T12): {bulk['docs']:,} documents in {s['wall_s']:.2f} s = {s['docs_per_s']:,.0f} docs/s, {s['cpu_ms_per_doc']:.3f} ms CPU/doc\n"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]) if len(sys.argv) > 1 else 2)
