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

"""The L2's faults inside one guest, built from faultlab netlab's Shape: per-peer netem out, drops in.

    net.py shape PEER [--ms MS] [--loss P] [--reorder P] [--duplicate P] [--mbit M]   (as root)
    net.py partition PEER | heal [PEER] | clear

Egress on `l2` is a prio qdisc with one band per peer: band 1 (the default, multicast included) is
unshaped, and peer j's address goes to band j + 1 by a u32 filter, where a netem qdisc carries the
direction's Shape. A partition adds the peer's address to the nftables set `cut`, whose input rule
drops what arrives from it: the sender sees silence and TCP retransmits after heal, as in netlab.
Run `partition` on both guests to cut both directions.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "faultlab" / "python"))

from faultlab.netlab import Shape

DEV = "l2"
L2 = "10.87.0"
PEERS = range(1, 16)  # prio has at most 16 bands
TABLE = "cluster"


def band(peer: int) -> str:
    """Peer's prio class: tc reads class and handle numbers in hex, so 1:a is band 10."""
    return f"1:{peer + 1:x}"


def leaf(peer: int) -> str:
    """The handle of peer's netem qdisc, also hex: 11: to 1f:."""
    return f"{peer + 0x10:x}:"


def run(argv: list[str], stdin: str | None = None) -> str:
    done = subprocess.run(argv, input=stdin, capture_output=True, text=True, check=False)
    if done.returncode:
        raise SystemExit(f"net.py: {' '.join(argv)}: exit {done.returncode}: {done.stderr.strip()}")
    return done.stdout


def setup() -> None:
    """Once per boot: the prio root, an unshaped netem per peer band with its filter, and the nftables table."""
    if " prio 1: " in run(["tc", "qdisc", "show", "dev", DEV]):
        return
    batch = [f"qdisc replace dev {DEV} root handle 1: prio bands 16 priomap {' '.join(['0'] * 16)}"]
    for peer in PEERS:
        batch.append(
            f"qdisc replace dev {DEV} parent {band(peer)} handle {leaf(peer)} netem {' '.join(Shape().netem())}"
        )
        batch.append(
            f"filter add dev {DEV} parent 1: protocol ip prio 1 u32 match ip dst {L2}.{peer}/32 flowid {band(peer)}"
        )
    run(["tc", "-batch", "-"], "\n".join(batch) + "\n")
    table = (
        f"table inet {TABLE} {{\n"
        "  set cut { type ipv4_addr; }\n"
        "  chain input { type filter hook input priority filter - 10; policy accept; ip saddr @cut drop; }\n"
        "}\n"
    )
    run(["nft", "-f", "-"], table)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(prog="net.py", description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("shape", help="replace the shape of what this guest sends to PEER")
    p.add_argument("peer", type=int, choices=PEERS)
    for name in ("ms", "loss", "reorder", "duplicate", "mbit"):
        p.add_argument(f"--{name}", type=float, default=0.0)
    sub.add_parser("partition", help="drop what arrives from PEER").add_argument("peer", type=int, choices=PEERS)
    sub.add_parser("heal", help="stop dropping PEER's packets (every peer's by default)").add_argument(
        "peer", type=int, nargs="?", choices=PEERS
    )
    sub.add_parser("clear", help="remove every shape and cut")
    args = parser.parse_args(argv)
    if args.command == "clear":
        subprocess.run(["tc", "qdisc", "del", "dev", DEV, "root"], capture_output=True, check=False)
        subprocess.run(["nft", "delete", "table", "inet", TABLE], capture_output=True, check=False)
        return 0
    setup()
    if args.command == "shape":
        shape = Shape(ms=args.ms, loss=args.loss, reorder=args.reorder, duplicate=args.duplicate, mbit=args.mbit)
        if shape.reorder and not shape.ms:
            raise SystemExit("net.py: reorder needs a delay: netem reorders by skipping it")
        handle = f"parent {band(args.peer)} handle {leaf(args.peer)}"
        run(["tc", "qdisc", "replace", "dev", DEV, *handle.split(), "netem", *shape.netem()])
    elif args.command == "partition":
        run(["nft", f"add element inet {TABLE} cut {{ {L2}.{args.peer} }}"])
    elif args.peer is not None:
        run(["nft", f"delete element inet {TABLE} cut {{ {L2}.{args.peer} }}"])
    else:
        run(["nft", f"flush set inet {TABLE} cut"])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
