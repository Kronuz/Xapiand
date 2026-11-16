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

"""TLS-transparent TCP links between cluster nodes: partitions, resets and bandwidth caps.

Each directed node pair (source, target) gets its own link: a listener on 127.0.0.1 that forwards
bytes to the target's peer port, so a fault on one link leaves the others alone. The proxy never
reads the bytes, so TLS passes through it unchanged.

python3 -I harness/cluster/proxy.py bandwidth [--mbit 100] [--mib 48] [--streams 1] [--tolerance 0.10]
"""

from __future__ import annotations

import argparse
import asyncio
import contextlib
import sys
import time

CHUNK = 65536
SLACK = 0.02  # seconds of idle wire a bucket keeps, so the copy loop's own latency is not lost rate


class Bucket:
    """Paces one direction of a link; every connection on the link shares it, as on one wire."""

    def __init__(self) -> None:
        self.rate = 0.0  # bytes per second; 0 is uncapped
        self.free_at = 0.0  # monotonic time the wire finishes the bytes already sent

    async def take(self, size: int) -> None:
        if self.rate <= 0:
            return
        now = time.monotonic()
        self.free_at = max(now - SLACK, self.free_at) + size / self.rate
        if self.free_at > now:
            await asyncio.sleep(self.free_at - now)


class Link:
    """One direction of a node pair's connections, forwarded to `target_port`."""

    def __init__(self, source: int, target: int, target_port: int) -> None:
        self.source = source
        self.target = target
        self.target_port = target_port
        self.port = 0
        self.open = asyncio.Event()
        self.open.set()
        self.outbound = Bucket()  # source to target
        self.inbound = Bucket()  # target to source
        self.server: asyncio.Server | None = None
        self.writers: set[asyncio.StreamWriter] = set()
        self.tasks: set[asyncio.Task[None]] = set()
        self.connections = 0

    async def start(self) -> int:
        self.server = await asyncio.start_server(self.accepted, "127.0.0.1", 0, limit=CHUNK)
        self.port = int(self.server.sockets[0].getsockname()[1])
        return self.port

    def cap(self, mbit: float) -> None:
        """Caps each direction at `mbit` megabits per second; 0 lifts the cap."""
        rate = mbit * 1_000_000 / 8
        self.outbound.rate = rate
        self.inbound.rate = rate

    def partition(self) -> None:
        """Holds every byte in flight and every new connection until heal: a silent network, not a reset."""
        self.open.clear()

    def heal(self) -> None:
        self.open.set()

    def reset(self) -> None:
        """Aborts every connection on the link, as a peer's RST would."""
        for writer in tuple(self.writers):
            writer.transport.abort()

    def accepted(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        task = asyncio.create_task(self.forward(reader, writer))
        self.tasks.add(task)
        task.add_done_callback(self.tasks.discard)

    async def forward(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        owned = [writer]
        self.writers.add(writer)
        try:
            await self.open.wait()
            upstream, output = await asyncio.wait_for(
                asyncio.open_connection("127.0.0.1", self.target_port, limit=CHUNK), 5
            )
            owned.append(output)
            self.writers.add(output)
            self.connections += 1
            await asyncio.gather(self.copy(reader, output, self.outbound), self.copy(upstream, writer, self.inbound))
        except (ConnectionError, OSError, TimeoutError):
            pass
        finally:
            for stream in owned:
                stream.close()
                self.writers.discard(stream)

    async def copy(self, source: asyncio.StreamReader, destination: asyncio.StreamWriter, bucket: Bucket) -> None:
        try:
            while payload := await source.read(CHUNK):
                await self.open.wait()
                await bucket.take(len(payload))
                await self.open.wait()
                destination.write(payload)
                await destination.drain()
            if destination.can_write_eof():
                destination.write_eof()
        except (ConnectionError, OSError):
            destination.transport.abort()

    async def close(self) -> None:
        if self.server is not None:
            self.server.close()
        for writer in tuple(self.writers):
            writer.transport.abort()
        for task in tuple(self.tasks):
            task.cancel()
        await asyncio.gather(*self.tasks, return_exceptions=True)
        if self.server is not None:
            await self.server.wait_closed()


class Mesh:
    """A link for every directed pair of nodes, numbered from 1."""

    def __init__(self) -> None:
        self.links: dict[tuple[int, int], Link] = {}

    async def start(self, peer_ports: dict[int, int]) -> None:
        for source in peer_ports:
            for target, port in peer_ports.items():
                if source != target:
                    link = Link(source, target, port)
                    self.links[source, target] = link
                    await link.start()

    def address(self, source: int, target: int) -> str:
        return f"127.0.0.1:{self.links[source, target].port}"

    def between(self, a: int, b: int) -> list[Link]:
        return [link for (s, t), link in self.links.items() if {s, t} == {a, b}]

    def touching(self, node: int) -> list[Link]:
        return [link for (s, t), link in self.links.items() if node in (s, t)]

    def isolate(self, node: int) -> None:
        for link in self.touching(node):
            link.partition()

    def heal(self, node: int | None = None) -> None:
        for link in self.links.values() if node is None else self.touching(node):
            link.heal()

    def cap(self, mbit: float) -> None:
        for link in self.links.values():
            link.cap(mbit)

    async def close(self) -> None:
        await asyncio.gather(*(link.close() for link in self.links.values()), return_exceptions=True)
        self.links.clear()


async def bandwidth(mbit: float, total: int, streams: int) -> float:
    """Streams `total` bytes split over `streams` connections through one capped link; returns Mb/s."""
    received = 0
    first = 0.0
    last = 0.0
    done = asyncio.Event()

    async def sink(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        nonlocal received, first, last
        while payload := await reader.read(CHUNK):
            now = time.monotonic()
            if not first:
                first = now
            received += len(payload)
            last = now
            if received >= total:
                done.set()
        writer.close()

    server = await asyncio.start_server(sink, "127.0.0.1", 0)
    link = Link(1, 2, int(server.sockets[0].getsockname()[1]))
    await link.start()
    link.cap(mbit)
    block = bytes(CHUNK)

    async def source(size: int) -> None:
        _, writer = await asyncio.open_connection("127.0.0.1", link.port)
        while size > 0:
            writer.write(block[: min(size, CHUNK)])
            size -= CHUNK
            await writer.drain()
        writer.close()

    try:
        share = total // streams
        await asyncio.gather(*(source(share if i else total - share * (streams - 1)) for i in range(streams)))
        await asyncio.wait_for(done.wait(), 600)
    finally:
        await link.close()
        server.close()
        with contextlib.suppress(Exception):
            await server.wait_closed()
    # The first chunk arrives after its own pacing slot, so the rate counts the bytes after it.
    return (received - CHUNK) * 8 / (last - first) / 1_000_000


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(prog="proxy.py", description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    bench = sub.add_parser("bandwidth", help="measure a capped link against its cap")
    bench.add_argument("--mbit", type=float, default=100.0)
    bench.add_argument("--mib", type=int, default=48, help="bytes to send, in MiB")
    bench.add_argument("--streams", type=int, default=1, help="connections sharing the link")
    bench.add_argument("--tolerance", type=float, default=0.10)
    args = parser.parse_args(argv)
    total = args.mib * 1024 * 1024
    started = time.monotonic()
    measured = asyncio.run(bandwidth(args.mbit, total, args.streams))
    error = measured / args.mbit - 1
    within = abs(error) <= args.tolerance
    print(
        f"bandwidth: cap {args.mbit:g} Mb/s, {args.mib} MiB over {args.streams} stream(s): "
        f"measured {measured:.2f} Mb/s ({error:+.2%}), {time.monotonic() - started:.1f} s; "
        f"{'within' if within else 'OUTSIDE'} {args.tolerance:.0%}"
    )
    return 0 if within else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
