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

"""The cluster harness's VM tier: N Linux guests under QEMU on one multicast L2 (README.md).

A run is a directory under CLUSTER_VM_ROOT holding each guest's qcow2 overlay on the base image, its
cloud-init seed, its logs and state.json. A guardian process in its own session owns the guests: each
QEMU runs under a keeper that stops it when the guardian's pipe closes, and the guardian closes them
all when the owner pid dies, at the deadline, on a signal, or when a keeper or guest stops.
"""

from __future__ import annotations

import argparse
import contextlib
import fcntl
import io
import json
import os
import select
import shutil
import signal
import socket
import subprocess
import sys
import tarfile
import threading
import time
from collections.abc import Callable, Sequence
from pathlib import Path
from types import FrameType
from typing import Any

HERE = Path(__file__).resolve().parent
ROOT = Path(os.environ.get("CLUSTER_VM_ROOT") or Path.home() / ".scratch" / "cluster-vm")
BASE = Path(os.environ.get("CLUSTER_VM_BASE") or Path.home() / ".scratch" / "vm-document-gate" / "linux" / "base.img")
FAULTLAB = Path(os.environ.get("CLUSTER_VM_FAULTLAB") or HERE / ".faultlab")
PORTS = range(18780, 18800)  # SSH forwards, one per guest
RESERVED = {18782, 18784}  # document-gate's Linux and FreeBSD VMs
USER = "cluster"
L2 = "10.87.0"  # guest i is 10.87.0.i on the L2
HOST_GROUP = "239.255.87.1"  # the host multicast group carrying the L2's frames; port = first SSH port + 10000
PROBE = ("239.192.87.1", 5870)  # the group the guests' probe uses on the L2
GUEST = "/opt/cluster"
DATA = "/srv/xapiand"
SSH_OPTIONS = ("-o", "UserKnownHostsFile=/dev/null", "-o", "StrictHostKeyChecking=no", "-o", "LogLevel=ERROR")
SSH_OPTIONS += ("-o", "ConnectTimeout=5", "-o", "BatchMode=yes", "-o", "ServerAliveInterval=5")


def log(message: str) -> None:
    print(f"vm.py: {message}", file=sys.stderr, flush=True)


class HarnessError(RuntimeError):
    pass


# Runs and their processes.


def read_state(run: Path) -> dict[str, object]:
    try:
        return json.loads((run / "state.json").read_text())
    except (OSError, ValueError):
        return {}


def write_state(run: Path, state: dict[str, object]) -> None:
    tmp = run / "state.json.tmp"
    tmp.write_text(json.dumps(state, indent=1) + "\n")
    tmp.replace(run / "state.json")


def ours(pid: int, run: Path) -> bool:
    """Whether `pid` is alive and one of `run`'s processes (its command line names the run directory)."""
    if pid <= 0:
        return False
    done = subprocess.run(["ps", "-p", str(pid), "-o", "command="], capture_output=True, text=True, check=False)
    return done.returncode == 0 and str(run) in done.stdout


def alive(pid: int) -> bool:
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def nodes_of(state: dict[str, object]) -> list[dict[str, int]]:
    nodes = state.get("nodes")
    return nodes if isinstance(nodes, list) else []


def int_of(state: dict[str, object], key: str) -> int:
    value = state.get(key)
    return value if isinstance(value, int) else 0


def pids_of(run: Path, node: dict[str, int]) -> list[int]:
    """The node's keeper and QEMU pids, the QEMU's from its pid file when the state has none yet."""
    qemu = node.get("qemu", 0)
    pidfile = run / f"qemu{node['index']}.pid"
    with contextlib.suppress(OSError, ValueError):
        qemu = qemu or int(pidfile.read_text())
    return [node.get("keeper", 0), qemu]


def discard_disks(run: Path) -> None:
    for path in run.glob("node*.qcow2"):
        path.unlink(missing_ok=True)
    for path in run.glob("seed*"):
        shutil.rmtree(path) if path.is_dir() else path.unlink(missing_ok=True)


def sweep_run(run: Path, reason: str) -> None:
    """Parks `run` without its guardian: SIGKILLs its keepers and guests by recorded pid, deletes its disks."""
    state = read_state(run)
    for node in nodes_of(state):
        for pid in pids_of(run, node):
            if ours(pid, run):
                with contextlib.suppress(ProcessLookupError):
                    os.kill(pid, signal.SIGKILL)
    discard_disks(run)
    if state and state.get("status") != "parked":
        state.update(status="parked", reason=reason)
        write_state(run, state)


def sweep(root: Path = ROOT) -> list[Path]:
    """Parks every run whose guardian died (SIGKILL runs nothing); returns them."""
    swept = []
    for run in sorted(root.glob("run-*")):
        state = read_state(run)
        if state and state.get("status") != "parked" and not ours(int_of(state, "guard"), run):
            sweep_run(run, "swept: its guardian died")
            swept.append(run)
    return swept


# The guardian and its keepers.


class Stop(Exception):
    pass


def _raise_stop(signum: int, frame: FrameType | None) -> None:
    raise Stop(signal.Signals(signum).name)


def keep(fd: int, pidfile: Path, argv: Sequence[str]) -> int:
    """Runs one QEMU and stops it when `fd` closes (the guardian parked or died) or on a signal."""
    for signum in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
        signal.signal(signum, _raise_stop)
    qemu = subprocess.Popen(argv, stdin=subprocess.DEVNULL)
    pidfile.write_text(f"{qemu.pid}\n")
    try:
        while qemu.poll() is None:
            if select.select([fd], [], [], 1.0)[0] and not os.read(fd, 64):
                break
    except Stop:
        pass
    finally:
        if qemu.poll() is None:
            qemu.terminate()
            try:
                qemu.wait(10)
            except subprocess.TimeoutExpired:
                qemu.kill()
                qemu.wait()
    return 0


def listening(port: int) -> bool:
    with contextlib.suppress(OSError), socket.create_connection(("127.0.0.1", port), timeout=1):
        return True
    return False


def guard(run: Path) -> int:
    """Owns `run`'s guests until the owner dies, the deadline passes, a signal comes or a guest stops."""
    for signum in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
        signal.signal(signum, _raise_stop)
    plan = json.loads((run / "plan.json").read_text())
    state: dict[str, object] = {"guard": os.getpid(), "status": "starting", "nodes": [], "mcast": plan["mcast"]}
    deadline = time.monotonic() + plan["timeout"]
    keepers: list[subprocess.Popen[bytes]] = []
    pipes: list[int] = []
    reason = "down"
    try:
        for node in plan["nodes"]:
            read, write = os.pipe()
            pidfile = run / f"qemu{node['index']}.pid"
            with (run / f"qemu{node['index']}.log").open("ab") as out:
                keeper = subprocess.Popen(
                    [
                        sys.executable,
                        "-E",
                        "-s",
                        str(Path(__file__)),
                        "keep",
                        str(read),
                        str(pidfile),
                        "--",
                        *node["argv"],
                    ],
                    pass_fds=(read,),
                    stdin=subprocess.DEVNULL,
                    stdout=out,
                    stderr=subprocess.STDOUT,
                )
            os.close(read)
            keepers.append(keeper)
            pipes.append(write)
            nodes_of(state).append({"index": node["index"], "port": node["port"], "keeper": keeper.pid, "qemu": 0})
        write_state(run, state)
        until = time.monotonic() + 30
        for node, keeper in zip(nodes_of(state), keepers, strict=True):
            pidfile = run / f"qemu{node['index']}.pid"
            while not (pidfile.exists() and listening(node["port"])):
                if keeper.poll() is not None or time.monotonic() > until:
                    raise Stop(f"node {node['index']}'s QEMU did not start (qemu{node['index']}.log)")
                time.sleep(0.2)
            node["qemu"] = int(pidfile.read_text())
        state["status"] = "started"
        write_state(run, state)
        while True:
            owner = (run / "owner").read_text().strip() if (run / "owner").exists() else ""
            if owner and not alive(int(owner)):
                raise Stop(f"owner {owner} died")
            if time.monotonic() > deadline:
                raise Stop(f"timeout after {plan['timeout']} s")
            for node, keeper in zip(nodes_of(state), keepers, strict=True):
                if keeper.poll() is not None:
                    raise Stop(f"node {node['index']} stopped")
            time.sleep(1)
    except Stop as stop:
        reason = str(stop)
    except Exception as error:
        reason = f"guardian error: {error!r}"
    finally:
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        signal.signal(signal.SIGHUP, signal.SIG_IGN)
        for write in pipes:
            os.close(write)
        until = time.monotonic() + 20
        for node, keeper in zip(nodes_of(state), keepers, strict=False):
            try:
                keeper.wait(max(0.1, until - time.monotonic()))
            except subprocess.TimeoutExpired:
                keeper.kill()
                keeper.wait()
            qemu = pids_of(run, node)[1]
            if ours(qemu, run):
                os.kill(qemu, signal.SIGKILL)
        discard_disks(run)
        state.update(status="parked", reason=reason)
        write_state(run, state)
        log(f"{run.name} parked: {reason}")
    return 0


# Booting a run.


def accelerator() -> list[str]:
    if sys.platform == "darwin":
        return ["-accel", "hvf", "-cpu", "host"]
    if os.access("/dev/kvm", os.R_OK | os.W_OK):
        return ["-accel", "kvm", "-cpu", "host"]
    return ["-accel", "tcg", "-cpu", "max"]


def free(port: int) -> bool:
    with socket.socket() as probe:
        try:
            probe.bind(("127.0.0.1", port))
        except OSError:
            return False
    return True


def ensure_key(root: Path) -> Path:
    key = root / "ssh_key"
    if not key.exists():
        subprocess.run(["ssh-keygen", "-q", "-t", "ed25519", "-N", "", "-C", "cluster-vm", "-f", str(key)], check=True)
    return key


def make_seed(run: Path, index: int, key: Path) -> Path:
    seed = run / f"seed{index}"
    seed.mkdir()
    (seed / "meta-data").write_text(f"instance-id: {run.name}-node{index}\nlocal-hostname: node{index}\n")
    (seed / "user-data").write_text(
        "#cloud-config\n"
        f"hostname: node{index}\n"
        "users:\n"
        f"  - name: {USER}\n"
        "    sudo: ALL=(ALL) NOPASSWD:ALL\n"
        "    shell: /bin/bash\n"
        f"    ssh_authorized_keys: [{(key.with_suffix('.pub')).read_text().strip()}]\n"
        "package_update: false\n"
        "package_upgrade: false\n"
    )
    (seed / "network-config").write_text(
        "version: 2\n"
        "ethernets:\n"
        f"  wan: {{match: {{macaddress: '52:54:00:87:00:{index:02x}'}}, set-name: wan, dhcp4: true}}\n"
        f"  l2: {{match: {{macaddress: '52:54:00:87:01:{index:02x}'}}, set-name: l2, addresses: ['{L2}.{index}/24'],\n"
        "       routes: [{to: 224.0.0.0/4, scope: link}]}\n"
    )
    iso = run / f"seed{index}.iso"
    if sys.platform == "darwin":
        argv = ["hdiutil", "makehybrid", "-quiet", "-o", str(iso), "-iso", "-joliet", "-default-volume-name", "cidata"]
        argv.append(str(seed))
    else:
        tool = shutil.which("genisoimage") or shutil.which("mkisofs") or shutil.which("xorrisofs")
        if tool is None:
            raise HarnessError("no genisoimage, mkisofs or xorrisofs for the cloud-init seed")
        argv = [tool, "-quiet", "-V", "cidata", "-J", "-r", "-o", str(iso), str(seed)]
    subprocess.run(argv, check=True)
    return iso


def qemu_argv(run: Path, index: int, port: int, mcast: str, cpus: int, mb: int) -> list[str]:
    return [
        shutil.which("qemu-system-x86_64") or "qemu-system-x86_64",
        *accelerator(),
        "-smp", str(cpus), "-m", str(mb),
        "-drive", f"file={run / f'node{index}.qcow2'},if=virtio,cache=unsafe,discard=unmap",
        "-drive", f"file={run / f'seed{index}.iso'},media=cdrom,readonly=on",
        "-netdev", f"user,id=wan,hostfwd=tcp:127.0.0.1:{port}-:22",
        "-device", f"virtio-net-pci,netdev=wan,mac=52:54:00:87:00:{index:02x}",
        "-netdev", f"socket,id=l2,mcast={mcast},localaddr=127.0.0.1",
        "-device", f"virtio-net-pci,netdev=l2,mac=52:54:00:87:01:{index:02x}",
        "-display", "none", "-monitor", "none", "-serial", f"file:{run / f'console{index}.log'}",
    ]  # fmt: skip


def ssh_argv(run: Path, index: int) -> list[str]:
    state = read_state(run)
    for node in nodes_of(state):
        if node["index"] == index:
            key = run.parent / "ssh_key"
            return ["ssh", "-i", str(key), "-p", str(node["port"]), *SSH_OPTIONS, f"{USER}@127.0.0.1"]
    raise HarnessError(f"{run} has no node {index}")


def ssh(run: Path, index: int, command: str, stdin: bytes | None = None, timeout: float = 600) -> str:
    done = subprocess.run(
        [*ssh_argv(run, index), command], input=stdin, capture_output=True, timeout=timeout, check=False
    )
    if done.returncode:
        raise HarnessError(f"node {index}: {command!r}: exit {done.returncode}: {done.stderr.decode().strip()}")
    return done.stdout.decode()


def guest_tar() -> bytes:
    """The guest side: guest/ and the pinned faultlab's Python, unpacked at /opt/cluster."""
    if not (FAULTLAB / "python" / "faultlab" / "disklab.py").exists():
        raise HarnessError(f"no faultlab checkout at {FAULTLAB} (cluster.sh pins and fetches it)")
    buffer = io.BytesIO()

    def skip(info: tarfile.TarInfo) -> tarfile.TarInfo | None:
        return None if "__pycache__" in info.name else info

    with tarfile.open(fileobj=buffer, mode="w") as tar:
        tar.add(HERE / "guest", "guest", filter=skip)
        tar.add(FAULTLAB / "python" / "faultlab", "faultlab/python/faultlab", filter=skip)
    return buffer.getvalue()


def parallel(nodes: Sequence[int], work: Callable[[int], str]) -> dict[int, str | BaseException]:
    results: dict[int, str | BaseException] = {}

    def one(index: int) -> None:
        try:
            results[index] = work(index)
        except BaseException as error:
            results[index] = error

    threads = [threading.Thread(target=one, args=(index,)) for index in nodes]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    return results


def provision(run: Path, index: int, tar: bytes, disk_mb: int, deadline: float) -> str:
    while True:
        try:
            ssh(run, index, "true", timeout=15)
            break
        except (HarnessError, subprocess.TimeoutExpired):
            if time.monotonic() > deadline or read_state(run).get("status") != "started":
                raise HarnessError(f"node {index}: no SSH (console{index}.log)") from None
            time.sleep(3)
    ssh(run, index, "cloud-init status --wait >/dev/null; true", timeout=max(30, deadline - time.monotonic()))
    ssh(run, index, f"sudo mkdir -p {GUEST} && sudo tar -xf - -C {GUEST}", stdin=tar)
    disk = f"/usr/bin/python3 -I {GUEST}/guest/disk.py"
    ssh(run, index, f"sudo systemd-run --quiet --collect --unit cluster-disk {disk} serve --data {DATA} --mb {disk_mb}")
    for _ in range(60):
        with contextlib.suppress(HarnessError):
            return ssh(run, index, f"{disk} ping").strip()
        time.sleep(0.5)
    raise HarnessError(f"node {index}: the disk agent does not answer (journalctl -u cluster-disk)")


def up(run: Path, nodes: int, owner: int, timeout: int, cpus: int, mb: int, disk_mb: int, boot: int) -> None:
    if not BASE.exists():
        raise HarnessError(f"no base image at {BASE} (set CLUSTER_VM_BASE)")
    tar = guest_tar()
    ROOT.mkdir(parents=True, exist_ok=True)
    for swept in sweep():
        log(f"swept {swept.name}")
    with (ROOT / ".lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)  # ports stay ours from choosing them until QEMU binds them
        key = ensure_key(ROOT)
        ports = [port for port in PORTS if port not in RESERVED and free(port)][:nodes]
        if len(ports) < nodes:
            raise HarnessError(f"only {len(ports)} free SSH ports in {PORTS.start}-{PORTS.stop - 1}")
        run.mkdir()
        (run / "owner").write_text(f"{os.getpid()}\n")
        mcast = f"{HOST_GROUP}:{ports[0] + 10000}"
        planned: list[dict[str, object]] = []
        for index, port in enumerate(ports, 1):
            backing = ["-b", str(BASE), "-F", "qcow2"]
            subprocess.run(
                ["qemu-img", "create", "-q", "-f", "qcow2", *backing, str(run / f"node{index}.qcow2"), "8G"], check=True
            )
            make_seed(run, index, key)
            argv = qemu_argv(run, index, port, mcast, cpus, mb)
            planned.append({"index": index, "port": port, "argv": argv})
        plan = {"mcast": mcast, "timeout": timeout, "nodes": planned}
        (run / "plan.json").write_text(json.dumps(plan, indent=1) + "\n")
        with (run / "guard.log").open("ab") as out:
            subprocess.Popen(
                [sys.executable, "-E", "-s", str(Path(__file__)), "guard", str(run)],
                start_new_session=True,
                stdin=subprocess.DEVNULL,
                stdout=out,
                stderr=subprocess.STDOUT,
            )
        while (status := read_state(run).get("status")) in (None, "starting"):
            time.sleep(0.2)
    if status != "started":
        raise HarnessError(f"{run.name}: {read_state(run).get('reason')} (see {run}/guard.log)")
    started = time.monotonic()
    log(f"{run.name}: {nodes} guests on SSH ports {', '.join(map(str, ports))}, L2 over {mcast}")
    results = parallel(range(1, nodes + 1), lambda index: provision(run, index, tar, disk_mb, started + boot))
    failed = {index: result for index, result in results.items() if isinstance(result, BaseException)}
    if failed:
        raise HarnessError("; ".join(str(error) for error in failed.values()))
    log(f"{run.name}: up in {time.monotonic() - started:.1f} s; data at {DATA} on disklab in each guest")
    (run / "owner").write_text(f"{owner}\n" if owner else "")


def down(run: Path) -> None:
    state = read_state(run)
    guardian = int_of(state, "guard")
    if ours(guardian, run):
        os.kill(guardian, signal.SIGTERM)
        until = time.monotonic() + 40
        while ours(guardian, run) and time.monotonic() < until:
            time.sleep(0.2)
    sweep_run(run, "down")


# Using a run.


def collect(run: Path, out: Path, paths: Sequence[str]) -> None:
    for node in nodes_of(read_state(run)):
        index = node["index"]
        target = out / f"node{index}"
        target.mkdir(parents=True, exist_ok=True)
        try:
            (target / "journal.txt").write_text(ssh(run, index, "sudo journalctl -b --no-pager", timeout=120))
            if paths:
                listed = " ".join(f"'{path}'" for path in paths)
                data = subprocess.run(
                    [*ssh_argv(run, index), f"sudo tar -cf - --ignore-failed-read {listed} 2>/dev/null"],
                    capture_output=True,
                    timeout=600,
                    check=False,
                ).stdout
                with tarfile.open(fileobj=io.BytesIO(data)) as tar:
                    tar.extractall(target, filter="data")
        except (HarnessError, subprocess.TimeoutExpired, tarfile.TarError) as error:
            log(f"collect node {index}: {error}")
    for name in (
        "guard.log",
        *(path.name for path in run.glob("console*.log")),
        *(p.name for p in run.glob("qemu*.log")),
    ):
        if (run / name).exists():
            shutil.copy(run / name, out / name)


def check(run: Path, out: Path, seconds: float, repeat: int, fill_mb: int) -> int:
    """A1: a probe datagram from each guest reaches every other over the L2, and in each guest a replay of
    a planted unsynced write finds it missing at the mark before its flush and present at the mark after."""
    nodes = [node["index"] for node in nodes_of(read_state(run))]
    group, port = PROBE
    probe = f"python3 -I {GUEST}/guest/probe.py --group {group} --port {port} --seconds {seconds}"
    results = parallel(nodes, lambda i: ssh(run, i, f"{probe} --addr {L2}.{i} --name node{i}", timeout=seconds + 60))
    multicast: dict[str, Any] = {}
    replays: dict[str, Any] = {}
    ok = True
    for index in nodes:
        result = results[index]
        heard: dict[str, str] = {}
        if isinstance(result, BaseException):
            log(f"multicast node{index}: {result}")
        else:
            heard = json.loads(result)["heard"]
        missing = [f"node{j}" for j in nodes if j != index and heard.get(f"node{j}") != f"{L2}.{j}"]
        ok &= not missing
        multicast[f"node{index}"] = {"heard": heard, "missing": missing}
        print(f"multicast node{index}: heard {sorted(heard)}, missing {missing or 'none'}", flush=True)
    timings: list[float] = []
    for index in nodes:
        plant = f"python3 -I {GUEST}/guest/disk.py plant --repeat {repeat} --fill {fill_mb}"
        try:
            replay: dict[str, Any] = json.loads(ssh(run, index, plant))
        except (HarnessError, ValueError) as error:
            replay = {"error": str(error), "before": [], "after": [], "seconds": []}
        before: list[str] = replay["before"]
        after: list[str] = replay["after"]
        passed = "error" not in replay and bool(before) and bool(after)
        passed = passed and "present" not in before and all(seen == "present" for seen in after)
        ok &= passed
        timings += replay["seconds"]
        replays[f"node{index}"] = replay
        error = f", {replay['error']}" if "error" in replay else ""
        print(
            f"replay node{index}: before {before}, after {after}, log {replay.get('log_mb')} MiB, "
            f"seconds per replay {replay['seconds']}{error}",
            flush=True,
        )
    summary: dict[str, Any] = {"run": run.name, "nodes": len(nodes), "multicast": multicast, "replay": replays}
    if timings:
        ordered = sorted(timings)
        median, worst = ordered[len(ordered) // 2], ordered[-1]
        summary["seconds_per_replay"] = {"median": median, "max": worst, "count": len(ordered)}
        print(f"seconds per replay: median {median:.3f}, max {worst:.3f} over {len(ordered)} replays", flush=True)
    summary["passed"] = ok
    out.mkdir(parents=True, exist_ok=True)
    (out / "summary.json").write_text(json.dumps(summary, indent=1) + "\n")
    print(f"A1 {'PASS' if ok else 'FAIL'} ({out / 'summary.json'})", flush=True)
    return 0 if ok else 1


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(prog="vm.py", description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    run_arg = {"type": Path, "default": os.environ.get("CLUSTER_VM_RUN"), "help": "the run (default $CLUSTER_VM_RUN)"}
    p = sub.add_parser("up", help="boot the guests; prints the run directory")
    p.add_argument("--nodes", type=int, default=3)
    p.add_argument("--owner", type=int, default=0, help="park the run when this pid dies")
    p.add_argument("--timeout", type=int, default=int(os.environ.get("CLUSTER_TIMEOUT", "1800")))
    p.add_argument("--cpus", type=int, default=2)
    p.add_argument("--mb", type=int, default=1536, help="each guest's memory")
    p.add_argument("--disk-mb", type=int, default=512, help="each guest's disklab device under the data directory")
    p.add_argument("--boot", type=int, default=300, help="seconds the guests may take to boot")
    p = sub.add_parser("down", help="park a run: stop its guests and delete their disks")
    p.add_argument("--run", **run_arg)
    sub.add_parser("sweep", help="park the runs whose guardian died")
    p = sub.add_parser("ssh", help="a command (or a shell) on guest I")
    p.add_argument("--run", **run_arg)
    p.add_argument("index", type=int)
    p.add_argument("cmd", nargs=argparse.REMAINDER)
    p = sub.add_parser("collect", help="each guest's journal and PATHs into OUT/nodeI, and the run's logs")
    p.add_argument("--run", **run_arg)
    p.add_argument("out", type=Path)
    p.add_argument("paths", nargs="*")
    p = sub.add_parser("check", help="A1: multicast between every pair of guests and a replay of a planted write")
    p.add_argument("--run", **run_arg)
    p.add_argument("--out", type=Path, default=Path(os.environ.get("CLUSTER_VM_OUT", "build/cluster-vm")))
    p.add_argument("--seconds", type=float, default=10)
    p.add_argument("--repeat", type=int, default=5, help="replays to each mark per guest")
    p.add_argument("--fill", type=int, default=128, help="MiB written and synced before the plant")
    p = sub.add_parser("guard")
    p.add_argument("run", type=Path)
    p = sub.add_parser("keep")
    p.add_argument("fd", type=int)
    p.add_argument("pidfile", type=Path)
    p.add_argument("argv", nargs=argparse.REMAINDER)
    args = parser.parse_args(argv)

    if args.command == "guard":
        return guard(args.run)
    if args.command == "keep":
        return keep(args.fd, args.pidfile, args.argv[1:] if args.argv[:1] == ["--"] else args.argv)
    if args.command == "sweep":
        swept = sweep()
        print(f"swept {len(swept)} run(s){': ' if swept else ''}{' '.join(run.name for run in swept)}")
        return 0
    if args.command == "up":
        signal.signal(signal.SIGTERM, _raise_stop)
        signal.signal(signal.SIGHUP, _raise_stop)
        run = ROOT / f"run-{time.strftime('%y%m%d-%H%M%S')}-{os.getpid()}"
        try:
            up(run, args.nodes, args.owner, args.timeout, args.cpus, args.mb, args.disk_mb, args.boot)
        except (HarnessError, Stop, KeyboardInterrupt, subprocess.CalledProcessError) as error:
            if run.exists():
                down(run)
            log(f"up failed: {error}")
            return 2
        print(run, flush=True)
        return 0
    if args.run is None:
        parser.error("no run: pass --run or set CLUSTER_VM_RUN")
    run = Path(args.run).resolve()
    if args.command == "down":
        down(run)
        return 0
    if args.command == "ssh":
        cmd = ssh_argv(run, args.index) + args.cmd
        os.execvp(cmd[0], cmd)
    if args.command == "collect":
        collect(run, args.out, args.paths)
        return 0
    if args.command == "check":
        return check(run, args.out, args.seconds, args.repeat, args.fill)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
