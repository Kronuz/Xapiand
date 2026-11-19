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

Peers are told apart by MAC address (guest i's `l2` is 52:54:00:87:01:i), so every protocol on the
L2 obeys, IPv4 and IPv6 alike. Egress on `l2` is a prio qdisc with one band per peer: band 1 (the
default, multicast and broadcast included) is unshaped, and a frame to peer j's MAC goes to band
j + 1 by a u32 filter, where a netem qdisc carries the direction's Shape. A partition adds the
peer's MAC to the nftables set `cut`, whose input rule drops what arrives from it: the sender sees
silence and TCP retransmits after heal, as in netlab. Run `partition` on both guests to cut both
directions.

The nftables table is built last, so its existence means the setup is complete: a command that
finds it missing clears what is there and builds everything again.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "faultlab" / "python"))

from faultlab.netlab import Shape

DEV = "l2"
MAC = "52:54:00:87:01"  # guest i's l2 is MAC:i, in hex
PEERS = range(1, 16)  # prio has at most 16 bands
TABLE = "cluster"


def mac(peer: int) -> str:
    return f"{MAC}:{peer:02x}"


def band(peer: int) -> str:
    """Peer's prio class: tc reads class and handle numbers in hex, so 1:a is band 10."""
    return f"1:{peer + 1:x}"


def leaf(peer: int) -> str:
    """The handle of peer's netem qdisc, also hex: 11: to 1f:."""
    return f"{peer + 0x10:x}:"


def run(argv: list[str], stdin: str | None = None, check: bool = True) -> str:
    done = subprocess.run(argv, input=stdin, capture_output=True, text=True, check=False)
    if done.returncode and check:
        raise SystemExit(f"net.py: {' '.join(argv)}: exit {done.returncode}: {done.stderr.strip()}")
    return "" if done.returncode else done.stdout


def clear() -> None:
    run(["tc", "qdisc", "del", "dev", DEV, "root"], check=False)
    run(["nft", "delete", "table", "inet", TABLE], check=False)


def setup() -> None:
    """The prio root, an unshaped netem per peer band with its filter, and the nftables table, in that
    order; the table's existence marks a complete setup, so anything less is cleared and redone."""
    if run(["nft", "list", "table", "inet", TABLE], check=False):
        return
    clear()
    batch = [f"qdisc replace dev {DEV} root handle 1: prio bands 16 priomap {' '.join(['0'] * 16)}"]
    for peer in PEERS:
        octets = bytes.fromhex(mac(peer).replace(":", ""))
        batch.append(
            f"qdisc replace dev {DEV} parent {band(peer)} handle {leaf(peer)} netem {' '.join(Shape().netem())}"
        )
        batch.append(
            f"filter add dev {DEV} parent 1: protocol all prio 1 u32 "
            f"match u16 0x{octets[:2].hex()} 0xffff at -14 match u32 0x{octets[2:].hex()} 0xffffffff at -12 "
            f"flowid {band(peer)}"
        )
    run(["tc", "-batch", "-"], "\n".join(batch) + "\n")
    table = (
        f"table inet {TABLE} {{\n"
        "  set cut { type ether_addr; }\n"
        "  chain input { type filter hook input priority filter - 10; policy accept; ether saddr @cut drop; }\n"
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
        clear()
        return 0
    setup()
    if args.command == "shape":
        shape = Shape(ms=args.ms, loss=args.loss, reorder=args.reorder, duplicate=args.duplicate, mbit=args.mbit)
        if shape.reorder and not shape.ms:
            raise SystemExit("net.py: reorder needs a delay: netem reorders by skipping it")
        handle = f"parent {band(args.peer)} handle {leaf(args.peer)}"
        run(["tc", "qdisc", "replace", "dev", DEV, *handle.split(), "netem", *shape.netem()])
    elif args.command == "partition":
        run(["nft", f"add element inet {TABLE} cut {{ {mac(args.peer)} }}"])
    elif args.peer is not None:
        element = f"element inet {TABLE} cut {{ {mac(args.peer)} }}"
        run(["nft", "-f", "-"], f"add {element}\ndelete {element}\n")  # the add makes heal idempotent
    else:
        run(["nft", f"flush set inet {TABLE} cut"])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
