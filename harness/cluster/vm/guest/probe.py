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


"""Sends this guest's name to a multicast group on the L2 and prints, as JSON, whom it heard from where.

    probe.py --group G --port P --addr L2_ADDRESS --name NAME [--seconds S]

It sends every 0.2 s for S seconds and listens 3 s longer, so guests started a little apart still
overlap. The datagrams leave by the L2 address's interface, so only the L2 can carry them.
"""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(prog="probe.py", description=__doc__.split("\n\n")[0])
    parser.add_argument("--group", required=True)
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--addr", required=True, help="this guest's address on the L2")
    parser.add_argument("--name", required=True)
    parser.add_argument("--seconds", type=float, default=10)
    args = parser.parse_args(argv)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((args.group, args.port))
    sock.setsockopt(
        socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, socket.inet_aton(args.group) + socket.inet_aton(args.addr)
    )
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(args.addr))
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 0)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)
    heard: dict[str, str] = {}
    sent = 0
    start = time.monotonic()
    send_at = 0.0
    while (now := time.monotonic() - start) < args.seconds + 3:
        if now >= send_at and now < args.seconds:
            sock.sendto(args.name.encode(), (args.group, args.port))
            sent += 1
            send_at = now + 0.2
        sock.settimeout(max(0.01, (send_at if now < args.seconds else args.seconds + 3) - now))
        try:
            datagram, (source, _) = sock.recvfrom(256)
        except TimeoutError:
            continue
        name = datagram.decode(errors="replace")
        if name != args.name:
            heard.setdefault(name, source)
    print(json.dumps({"name": args.name, "sent": sent, "heard": heard}))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
