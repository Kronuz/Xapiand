#!/usr/bin/env python3
"""closing_units.py -- the closing benchmark's units, their packing into runs, and a unit's arms
(docs/design/closing-benchmark.md, D10 and D14). Reached through closing_bench.sh.

  closing_units.py list
  closing_units.py plan [--times FILE] [--resume-min M] [--approved USD] [--bundle DIR]
      First-fit-decreasing packing of whole units into `lease run`s of at most 45 minutes, phase by phase. A unit longer
      than the content budget is refused, not shrunk. FILE has `unit minutes` lines (the dry run's measurements).
  closing_units.py run --units a,b,c --deadline-min N --root RESULTS [--smoke]
      Runs the units in order. A unit whose estimate does not fit before the deadline is skipped and recorded, since an
      overrun loses the whole run's $OUT (CB-R14). Each arm runs from a stopped process with the page cache dropped,
      in rotated order, and leaves ROOT/UNIT/ARM/rN.json and rN.rows.json.

Data lives under $CLOSING_DATA (default ~/data): corpora/, indexes/CORPUS/ARM/ (built once by B1 to B3), work/.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

import closing_report as cr
import closing_rows as rows

HERE = Path(__file__).resolve().parent
MAX_RUN_MIN = 45.0
SPEC = {  # arm -> (system, port)
    "S": ("xapiand", 18970),
    "Sp": ("xapiand", 18970),
    "X": ("xapiand", 18971),
    "E": ("es", 19270),
    "Q": ("quickwit", 17270),
}
ARMS = list(SPEC)
TENANT_ARMS = {  # arm -> (system, shape, port)
    "S": ("xapiand", "per-index", 18970),
    "Sp": ("xapiand", "per-index", 18970),
    "X": ("xapiand", "per-index", 18971),
    "E-index": ("es", "per-index", 19270),
    "E-shared": ("es", "shared", 19270),
    "Q-shared": ("quickwit", "shared", 17270),
    "Q-index": ("quickwit", "per-index", 17270),
}
BUILDS = {"wiki": "wiki.ndjson", "logs": "logs.ndjson"}


@dataclass(frozen=True)
class Unit:
    id: str
    phase: str  # A setup, B builds, C calibration, D confirm, E report
    row: str  # the row the unit measures (t1 ... t8, build, setup)
    round: int | None
    minutes: float  # an estimate until the dry run measures it


def all_units() -> list[Unit]:
    units = [
        Unit("M0a", "A", "setup", None, 25),
        Unit("M0b", "A", "setup", None, 40),
        Unit("M1", "A", "setup", None, 15),
    ]
    units += [Unit(f"B{i}", "B", "build", None, m) for i, m in ((1, 40), (2, 40), (3, 35), (4, 40), (5, 40))]
    per_round = {
        "t1": 12,
        "t2": 15,
        "t4a": 25,
        "t4b": 30,
        "t5a": 30,
        "t5b": 30,
        "t6": 35,
        "t7": 30,
        "t8-1e4": 30,
        "t8-1e5": 40,
    }
    for n in (1, 2, 3, 4, 5):
        for row, m in per_round.items():
            phase = "C" if n <= 2 else "D"
            units.append(Unit(f"{row}-r{n}", phase, row, n, m if n <= 2 else m * 0.7))
    units += [Unit(f"E{i}", "E", "report", None, m) for i, m in ((1, 30), (2, 30), (3, 25), (4, 40))]
    return units


@dataclass
class Run:
    phase: str
    units: list[Unit]

    @property
    def minutes(self) -> float:
        return sum(u.minutes for u in self.units)


def pack(units: list[Unit], budget: float) -> list[Run]:
    """First-fit decreasing inside each phase; a unit over the budget is an error (a unit never splits)."""
    runs: list[Run] = []
    for phase in "ABCDE":
        bins: list[Run] = []
        for u in sorted((u for u in units if u.phase == phase), key=lambda u: -u.minutes):
            if u.minutes > budget:
                raise SystemExit(f"{u.id} takes {u.minutes:.0f} min, over the {budget:.0f}-minute budget of a run")
            slot = next((b for b in bins if b.minutes + u.minutes <= budget), None)
            if slot is None:
                slot = Run(phase, [])
                bins.append(slot)
            slot.units.append(u)
        runs += bins
    return runs


def read_times(path: str | None, units: list[Unit]) -> list[Unit]:
    if not path:
        return units
    times: dict[str, float] = {}
    for line in Path(path).read_text().splitlines():
        if line.strip() and not line.startswith("#"):
            name, minutes = line.split()[:2]
            times[name] = float(minutes)
    return [Unit(u.id, u.phase, u.row, u.round, times.get(u.id, u.minutes)) for u in units]


def plan(args: argparse.Namespace) -> int:
    units = read_times(args.times, all_units())
    overhead = args.resume_min + args.sync_min + args.setup_min + args.archive_min + args.margin_min
    budget = MAX_RUN_MIN - overhead
    runs = pack(units, budget)
    hours = cost = 0.0
    for i, r in enumerate(runs, 1):
        t = int(min(MAX_RUN_MIN, math.ceil(r.minutes + overhead)))
        hours += (t + 5) / 60  # a run's worst case: its timeout plus the stop margin
        ids = ",".join(u.id for u in sorted(r.units, key=lambda u: (u.round or 0, u.id)))
        cmd = (
            f"lease run closing-metal c7i.metal-24xl --timeout {t}m --sync {args.bundle} "
            f"--setup {args.bundle}/harness/closing/setup.sh -- harness/closing_bench.sh --units {ids} "
            f"--deadline-min {t - overhead:.0f}"
        )
        print(f"run {i:2d} phase {r.phase} {r.minutes:5.1f} min of {budget:.0f}: {cmd}")
    cost = hours * args.rate
    print(
        f"\n{len(runs)} runs, {hours:.1f} machine-hours, ${cost:.0f} on-demand at ${args.rate}/h (worst case per run)"
    )
    if args.approved and cost > args.approved * 1.2:
        print(f"OVER: more than 20% above the approved ${args.approved:.0f}; it goes back to the owner (D9)")
        return 3
    return 0


# -- the arms --------------------------------------------------------------------------------------


class Host:
    """The directories, binaries and cpusets of one machine."""

    def __init__(self, smoke: bool) -> None:
        self.data = Path(os.environ.get("CLOSING_DATA", os.path.expanduser("~/data")))
        self.smoke = smoke
        self.bins = {
            "xapiand": os.environ.get("CLOSING_XAPIAND", str(self.data / "xapiand" / "xapiand")),
            "x": os.environ.get("CLOSING_X", str(self.data / "xapiand-x" / "xapiand")),
            "es": os.environ.get("CLOSING_ES", str(self.data / "elasticsearch-9.3.4" / "bin" / "elasticsearch")),
            "quickwit": os.environ.get("CLOSING_QUICKWIT", str(self.data / "quickwit-v0.9.3" / "quickwit")),
        }
        sys_cores, gen_cores = (2, 4) if smoke else (8, 16)
        self.sys_cpus, self.gen_cpus = cpusets(sys_cores, gen_cores)
        self.all_cpus = self.sys_cpus.split(",") + self.gen_cpus.split(",")
        self.sys_n = sys_cores

    def binary(self, arm: str) -> str:
        return self.bins["x"] if arm == "X" else self.bins[SPEC.get(arm, TENANT_ARMS.get(arm, ("", "")))[0]]

    def prefix(self) -> str:
        if os.environ.get("CLOSING_NO_CGROUP"):
            return ""
        return (
            f"sudo -E systemd-run --scope --quiet --uid={os.getuid()} --gid={os.getgid()} "
            f"-p AllowedCPUs={self.sys_cpus} -p MemoryMax={'4G' if self.smoke else '32G'} --"
        )

    def gen(self, argv: list[str]) -> list[str]:
        return ["taskset", "-c", self.gen_cpus, *argv] if shutil.which("taskset") else argv


def cpusets(sys_cores: int, gen_cores: int) -> tuple[str, str]:
    """One hardware thread of each of the first physical cores: the system's, then the generators'."""
    out = subprocess.run(["lscpu", "-p=CPU,CORE,SOCKET"], capture_output=True, text=True, check=False).stdout
    seen: dict[tuple[str, str], str] = {}
    for line in out.splitlines():
        if line.startswith("#") or not line.strip():
            continue
        cpu, core, sock = line.split(",")
        seen.setdefault((sock, core), cpu)
    cpus = list(seen.values())
    if len(cpus) < sys_cores + gen_cores:
        sys_cores, gen_cores = max(1, len(cpus) // 3), max(1, len(cpus) - len(cpus) // 3)
    return ",".join(cpus[:sys_cores]), ",".join(cpus[sys_cores : sys_cores + gen_cores])


def drop_caches() -> None:
    subprocess.run(["sync"], check=False)
    subprocess.run(["sudo", "sh", "-c", "echo 3 > /proc/sys/vm/drop_caches"], check=False)


def run_tool(host: Host, argv: list[str], log: Path) -> int:
    log.parent.mkdir(parents=True, exist_ok=True)
    with open(log, "wb") as fh:
        return subprocess.run(host.gen(argv), stdout=fh, stderr=subprocess.STDOUT, check=False).returncode


def common(host: Host, arm: str, work: Path, out: Path, extra_workdir: Path | None = None) -> list[str]:
    system, port = SPEC[arm]
    return [
        sys.executable,
        "--system",
        system,
        "--bin",
        host.binary(arm),
        "--workdir",
        str(extra_workdir or work),
        "--target",
        f"127.0.0.1:{port}",
        "--cpus",
        str(host.sys_n),
        "--launch-prefix",
        host.prefix(),
        "--assert-settings",
        "--out",
        str(out),
    ]


def tool(name: str, argv: list[str]) -> list[str]:
    """argv is [python, flags...]: insert the script after the interpreter."""
    return [argv[0], str(HERE / name), *argv[1:]]


def subset(source: Path, classes: tuple[str, ...], dest: Path) -> str:
    """The queries of the given classes, written beside the work directory (stall_load runs a file's every class)."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    lines = [ln for ln in source.read_text().splitlines() if ln.strip() and json.loads(ln)["class"] in classes]
    dest.write_text("\n".join(lines) + "\n")
    return str(dest)


def restore(host: Host, arm: str, work: Path) -> Path:
    """A pristine copy of the arm's full C-logs index (made in B3) in its own work directory (D10: T6 and T7)."""
    shutil.rmtree(work, ignore_errors=True)
    work.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["cp", "-a", "--reflink=auto", str(host.data / "indexes" / "logs" / arm.replace("Sp", "S")), str(work)],
        check=True,
    )
    return work


def settings(host: Host) -> dict[str, int]:
    path = host.data / "settings.json"
    base = {"clients": 4, "batch_docs": 1000, "shards_s": 1, "shards_x": 1}
    return {**base, **json.loads(path.read_text())} if path.exists() else base


def shards(arm: str, st: dict[str, int]) -> str:
    return str(st["shards_x"] if arm == "X" else st["shards_s"])


def corpus(host: Host, name: str, limit: int) -> str:
    sample = host.data / "corpora" / f"sample-{limit}" / name if limit else None
    return str(sample if sample and sample.exists() else host.data / "corpora" / name)


def unit_arm_command(host: Host, unit: Unit, arm: str, root: Path) -> tuple[list[str], str, Path, Path]:
    """The tool's argv, its rows kind, the JSON it writes and the work directory to remove afterwards."""
    n = unit.round or 0
    out = root / unit.id.rsplit("-r", 1)[0] / arm / f"r{n}.json"
    work = host.data / "work" / f"{unit.id}-{arm}"
    st = settings(host)
    lim = 20000 if host.smoke else 0
    logs, wiki = corpus(host, "logs.ndjson", lim), corpus(host, "wiki.ndjson", lim)
    queries = host.data / "queries"
    if unit.row in ("t1", "t2"):
        _name, schema, path, cut = (
            ("wiki-500k", "wiki", wiki, 500_000) if unit.row == "t1" else ("logs-1M", "logs", logs, 1_000_000)
        )
        argv = [
            *common(host, arm, work, out),
            "--corpus", path, "--schema", schema, "--limit", str(min(cut, lim) if lim else cut),
            "--clients", str(st["clients"]), "--batch-docs", str(st["batch_docs"]), "--shards", shards(arm, st), "--duration", "0",
        ]  # fmt: skip
        return tool("loadtest.py", argv), unit.row, out, work
    if unit.row in ("t4a", "t4b"):
        corpus_name, schema = ("wiki", "wiki") if unit.row == "t4a" else ("logs", "logs")
        argv = [
            *common(host, arm, work, out, host.data / "indexes" / corpus_name / arm.replace("Sp", "S")),
            "--keep", "--schema", schema, "--index", corpus_name, "--queries", str(queries / f"{corpus_name}.jsonl"),
            "--latency", "--runs", "10", "--warm-secs", "5" if host.smoke else "60", "--restart", "--drop-caches", "--duration", "0",
        ]  # fmt: skip
        return tool("loadtest.py", argv), "t4", out, Path()
    if unit.row in ("t5a", "t5b"):
        corpus_name, classes = (
            ("wiki", ("term", "phrase")) if unit.row == "t5a" else ("logs", ("term_range", "agg_histogram"))
        )
        argv = [
            *common(host, arm, work, out, host.data / "indexes" / corpus_name / arm.replace("Sp", "S")),
            "--keep", "--schema", corpus_name, "--index", corpus_name,
            "--queries", subset(queries / f"{corpus_name}.jsonl", classes, work.with_suffix(".q.jsonl")),
            "--class", "all", "--saturate", "--procs", str(max(1, len(host.gen_cpus.split(",")) // 2)),
            *(["--saturate-start", "100", "--saturate-max", "800", "--bisect-steps", "1", "--step-secs", "4"] if host.smoke else []),
        ]  # fmt: skip
        return tool("stall_load.py", argv), "t5", out, Path()
    if unit.row == "t6":
        pristine = restore(host, arm, work)
        argv = [
            *common(host, arm, work, out, pristine),
            "--keep", "--mixed", "--schema", "logs", "--index", "logs",
            "--queries", subset(queries / "logs.jsonl", ("term_range",), work.with_suffix(".q.jsonl")),
            "--write-corpus", corpus(host, "logs-writes.ndjson", lim), "--write-rate", str(st.get("write_rate", 5000)),
            "--write-batch", "100", "--rate", str(st.get("search_rate", 200)),
            "--duration", "20" if host.smoke else "180", "--warmup", "5" if host.smoke else "30",
            "--procs", str(max(1, len(host.gen_cpus.split(",")) // 2)),
        ]  # fmt: skip
        return tool("stall_load.py", argv), "t6", out, pristine
    if unit.row == "t7":
        pristine = restore(host, arm, work)
        system, port = SPEC[arm]
        argv = [
            sys.executable, str(HERE / "engine_mixed_load.py"), host.binary(arm), str(pristine),
            "--system", system, "--target", f"127.0.0.1:{port}", "--cpus", str(host.sys_n),
            "--launch-prefix", host.prefix(), "--keep", "--index", "logs", "--schema", "logs",
            "--stream-corpus", corpus(host, "logs-writes.ndjson", lim), "--write-rate", str(st.get("write_rate", 5000)),
            "--kill-at", "15" if host.smoke else "60", "--secs", "40" if host.smoke else "90",
            "--sent-log", str(work.with_suffix(".sent.tsv")), "--result", str(out),
        ]  # fmt: skip
        return argv, "t7crash", out, pristine
    raise SystemExit(f"unit {unit.id}: the {unit.row} row is not run by closing_units yet")


def build_unit(host: Host, unit: Unit, root: Path) -> list[str]:
    """B1 (C-wiki) and B2 (C-logs): the full index of every system, built together in disjoint cpusets, each load
    resumable (a rerun continues) and ending with merges settled (C7, load_index). The indexes stay in indexes/."""
    name = {"B1": "wiki", "B2": "logs"}.get(unit.id)
    if name is None:
        raise SystemExit(f"unit {unit.id}: not built by closing_units yet")
    st = settings(host)
    cpus = host.all_cpus
    per = max(1, host.sys_n // 4) if host.smoke else host.sys_n
    arms = ["S", "X", "E", "Q"]
    procs: list[tuple[str, subprocess.Popen[bytes]]] = []
    lim = 20000 if host.smoke else 0
    for k, arm in enumerate(arms):
        mine = ",".join(cpus[k * per : (k + 1) * per])
        gen = ",".join(cpus[len(arms) * per :]) or ",".join(cpus)
        system, port = SPEC[arm]
        index_dir = host.data / "indexes" / name / arm
        index_dir.parent.mkdir(parents=True, exist_ok=True)
        out = root / unit.id / arm / "build.json"
        prefix = f"sudo -E systemd-run --scope --quiet --uid={os.getuid()} --gid={os.getgid()} -p AllowedCPUs={mine} --"
        argv = [
            sys.executable, str(HERE / "loadtest.py"), "--system", system, "--bin", host.binary(arm), "--workdir", str(index_dir),
            "--target", f"127.0.0.1:{port}", "--cpus", str(per), "--launch-prefix", prefix, "--index", name, "--schema", name,
            "--corpus", corpus(host, f"{name}.ndjson", lim), "--clients", str(st["clients"]), "--batch-docs", str(st["batch_docs"]),
            "--shards", shards(arm, st), "--resume", str(index_dir.parent / f"{arm}.state"), "--duration", "0", "--out", str(out),
        ]  # fmt: skip
        out.parent.mkdir(parents=True, exist_ok=True)
        fh = open(out.with_suffix(".log"), "wb")
        procs.append(
            (
                arm,
                subprocess.Popen(
                    ["taskset", "-c", gen, *argv] if shutil.which("taskset") else argv, stdout=fh, stderr=fh
                ),
            )
        )
    ran = []
    for arm, p in procs:
        if p.wait() == 0:
            ran.append(arm)
        else:
            print(f"{unit.id} {arm}: the build exited {p.returncode}", file=sys.stderr)
    queries = host.data / "queries"
    queries.mkdir(parents=True, exist_ok=True)
    src = (
        ["wiki", "--source", str(host.data / "corpora" / "queries.txt")]
        if name == "wiki"
        else ["logs", "--corpus", corpus(host, "logs.ndjson", lim)]
    )
    subprocess.run(
        [sys.executable, str(HERE / "closing_queries.py"), *src, "--out", str(queries / f"{name}.jsonl")], check=False
    )
    return ran


def run_unit(host: Host, unit: Unit, root: Path, all_arms: list[str]) -> list[str]:
    """Run one unit's arms: every arm in rounds 1 and 2, the anchors and their near arms after. Returns the arms run."""
    if unit.row == "build":
        return build_unit(host, unit, root)
    row_dir = unit.id.rsplit("-r", 1)[0]
    n = unit.round or 1
    data = cr.collect(root, row_dir)
    arms = cr.arms_for_round(data, n, all_arms)
    k = (n - 1) % len(arms)
    arms = arms[k:] + arms[:k]  # each round starts one arm later (S X E Q S', then X E Q S' S, ...)
    ran = []
    for arm in arms:
        argv, kind, out, work = unit_arm_command(host, unit, arm, root)
        drop_caches()
        t0 = time.time()
        rc = run_tool(host, argv, out.with_suffix(".log"))
        if rc != 0 or not out.exists():
            print(f"{unit.id} {arm}: the tool exited {rc}; see {out.with_suffix('.log')}", file=sys.stderr)
            continue
        doc = json.loads(out.read_text())
        doc["wall_s"] = round(time.time() - t0, 1)
        out.write_text(json.dumps(doc, indent=1))
        cr.write_rows(out.with_suffix(".rows.json").with_name(f"r{n}.rows.json"), rows.rows_for(kind, doc))
        if kind == "t4":  # the same run's clean restart is T7's clean row
            clean = root / ("t7-" + row_dir) / arm / f"r{n}.rows.json"
            cr.write_rows(clean, rows.rows_for("t7", doc))
        if work != Path() and work.exists():
            shutil.rmtree(work, ignore_errors=True)
        ran.append(arm)
    return ran


def fits(estimate_s: float, end: float, now: float | None = None) -> bool:
    """Whether a unit with this estimate finishes before the deadline."""
    return (time.time() if now is None else now) + estimate_s <= end


def run(args: argparse.Namespace) -> int:
    host = Host(args.smoke)
    by_id = {u.id: u for u in read_times(args.times, all_units())}
    root = Path(args.root)
    root.mkdir(parents=True, exist_ok=True)
    end = time.time() + args.deadline_min * 60
    skipped = root / "skipped.tsv"
    for uid in args.units.split(","):
        unit = by_id.get(uid)
        if unit is None:
            raise SystemExit(f"unknown unit {uid}")
        est = unit.minutes * 60
        if not fits(est, end):
            with open(skipped, "a") as fh:
                fh.write(f"{uid}\testimate {est:.0f}s, {end - time.time():.0f}s left\n")
            print(f"{uid}: skipped, the estimate does not fit before the deadline", file=sys.stderr)
            continue
        t0 = time.time()
        ran = run_unit(host, unit, root, ARMS if not unit.row.startswith("t8") else list(TENANT_ARMS))
        with open(root / "phases.tsv", "a") as fh:
            fh.write(f"{uid}\t{time.time() - t0:.1f}\t{' '.join(ran)}\n")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("list")
    p = sub.add_parser("plan")
    p.add_argument("--times")
    p.add_argument("--resume-min", type=float, default=1.0, help="M0's measured resume (39 s on one launch)")
    p.add_argument("--sync-min", type=float, default=0.5)
    p.add_argument("--setup-min", type=float, default=1.0)
    p.add_argument("--archive-min", type=float, default=1.0)
    p.add_argument("--margin-min", type=float, default=1.5)
    p.add_argument("--rate", type=float, default=4.284, help="$/hour of the c7i.metal-24xl")
    p.add_argument("--approved", type=float, default=0.0, help="the owner's approved total, USD")
    p.add_argument("--bundle", default="BUNDLE")
    r = sub.add_parser("run")
    r.add_argument("--units", required=True)
    r.add_argument("--deadline-min", type=float, required=True)
    r.add_argument("--root", required=True)
    r.add_argument("--times")
    r.add_argument(
        "--smoke", action="store_true", help="a reduced run for a small machine: 2 + 4 cores, 20,000 documents"
    )
    args = ap.parse_args()
    if args.cmd == "list":
        for u in all_units():
            print(f"{u.id}\t{u.phase}\t{u.minutes:g}")
        return 0
    return plan(args) if args.cmd == "plan" else run(args)


if __name__ == "__main__":
    sys.exit(main())
