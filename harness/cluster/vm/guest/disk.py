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


"""A guest's data directory on faultlab's disklab: dm-log-writes under it, marks, power cuts and replays.

    disk.py serve --data DIR [--mb N]   (root, under systemd-run) owns the lab; DIR links to its filesystem
    disk.py ping | mark NAME | stop | replay TO | unreplay | points    one request to the server, JSON out
    disk.py plant [--repeat R] [--fill MB]   a write left unsynced between two marks, then the power cut

A lab lives as long as the process that opened it (disklab tears it down at exit), so one server per
guest owns it and every other process asks it over a Unix socket.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pwd
import socket
import socketserver
import sys
import time
from pathlib import Path
from typing import override

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "faultlab" / "python"))

from faultlab.disklab import Disklab

SOCKET = "/run/cluster-disk.sock"
OWNER = "cluster"  # the SSH user, who owns the data directory


def serve(data: Path, mb: int) -> int:
    lab = Disklab(prefix="cluster")
    mnt = lab.open(mb=mb)
    user = pwd.getpwnam(OWNER)
    os.chown(mnt, user.pw_uid, user.pw_gid)
    if data.is_symlink() or data.exists():
        data.unlink()
    data.parent.mkdir(parents=True, exist_ok=True)
    data.symlink_to(mnt)

    def answer(op: str, arg: str | None) -> dict[str, object]:
        if op == "ping":
            return {"lab": lab.name, "data": str(data), "mnt": str(mnt), "mounted": lab.mounted, "mb": mb}
        if op == "mark" and arg:
            lab.mark(arg)
            return {"mark": arg}
        if op == "stop":
            entries = lab.stop()
            size = (lab.dir / "log.img").stat().st_blocks * 512
            return {"entries": len(entries), "points": len(lab.points()), "log_mb": round(size / (1 << 20), 1)}
        if op == "replay" and arg:
            path = lab.replay(int(arg) if arg.isdigit() else arg)
            return {"replay": str(path)}
        if op == "unreplay":
            lab.unmount_replay()
            return {}
        if op == "points":
            return {"points": [entry.describe() for entry in lab.points()]}
        return {"error": f"unknown request {op} {arg or ''}".strip()}

    class Handler(socketserver.StreamRequestHandler):
        @override
        def handle(self) -> None:
            request = json.loads(self.rfile.readline())
            try:
                reply = answer(request["op"], request.get("arg"))
            except Exception as error:
                reply = {"error": f"{type(error).__name__}: {error}"}
            self.wfile.write(json.dumps(reply).encode() + b"\n")

    Path(SOCKET).unlink(missing_ok=True)
    with socketserver.UnixStreamServer(SOCKET, Handler) as server:
        os.chmod(SOCKET, 0o666)
        print(f"disk: {lab.name} at {mnt}, {data} -> it, {mb} MiB", flush=True)
        server.serve_forever()
    return 0


def ask(op: str, arg: str | None = None) -> dict[str, object]:
    with socket.socket(socket.AF_UNIX) as client:
        client.connect(SOCKET)
        client.sendall(json.dumps({"op": op, "arg": arg}).encode() + b"\n")
        reply = json.loads(client.makefile("rb").readline())
    if "error" in reply:
        raise RuntimeError(f"{op}: {reply['error']}")
    return reply


def state(path: Path, digest: str) -> str:
    """What a replay holds of the planted file: present, absent, empty, zeros or other."""
    if not path.exists():
        return "absent"
    content = path.read_bytes()
    if hashlib.sha256(content).hexdigest() == digest:
        return "present"
    return "empty" if not content else "zeros" if not content.strip(b"\0") else "other"


def plant(repeat: int, fill_mb: int) -> dict[str, object]:
    """A1's power cut: a write left unsynced at mark `before`, fsynced by mark `after`, then each replayed."""
    data = Path(str(ask("ping")["mnt"]))
    block = os.urandom(1 << 20)
    for n in range(fill_mb):  # a log the size a short run leaves, synced as a node syncs
        fd = os.open(data / f"fill-{n}", os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
        os.write(fd, block)
        os.fsync(fd)
        os.close(fd)
    payload = os.urandom(64 << 10)
    digest = hashlib.sha256(payload).hexdigest()
    target = data / "planted"
    target.write_bytes(payload)  # no fsync: only the page cache holds it
    ask("mark", "before")
    fd = os.open(target, os.O_RDONLY)
    os.fsync(fd)
    os.close(fd)
    ask("mark", "after")
    stopped = ask("stop")
    result: dict[str, object] = {"before": [], "after": [], "seconds": [], **stopped}
    for _ in range(repeat):
        for mark in ("before", "after"):
            started = time.monotonic()
            replay = Path(str(ask("replay", mark)["replay"]))
            seen = state(replay / "planted", digest)
            ask("unreplay")
            result[mark].append(seen)  # pyrefly: ignore[missing-attribute]
            result["seconds"].append(round(time.monotonic() - started, 3))  # pyrefly: ignore[missing-attribute]
    return result


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(prog="disk.py", description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("serve")
    p.add_argument("--data", type=Path, required=True)
    p.add_argument("--mb", type=int, default=512)
    for name in ("ping", "stop", "unreplay", "points"):
        sub.add_parser(name)
    sub.add_parser("mark").add_argument("arg")
    sub.add_parser("replay").add_argument("arg")
    p = sub.add_parser("plant")
    p.add_argument("--repeat", type=int, default=5)
    p.add_argument("--fill", type=int, default=128)
    args = parser.parse_args(argv)
    if args.command == "serve":
        return serve(args.data, args.mb)
    if args.command == "plant":
        print(json.dumps(plant(args.repeat, args.fill)))
        return 0
    print(json.dumps(ask(args.command, getattr(args, "arg", None))))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
