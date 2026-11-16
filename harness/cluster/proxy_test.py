"""A partitioned link delivers nothing, and healing delivers what it held, in order.

For each event (bytes, a FIN, a reset, from either side, and a new connection), a connection
through one link sees the event happen on its side during a partition; the other side must see
nothing for a second, then the event after `heal()`.

harness/cluster/run.sh proxytest
"""

from __future__ import annotations

import asyncio
import socket
import struct
import sys

from proxy import Link

QUIET = 1.0

type Stream = tuple[asyncio.StreamReader, asyncio.StreamWriter]


async def seen(reader: asyncio.StreamReader, seconds: float) -> str:
    try:
        data = await asyncio.wait_for(reader.read(65536), seconds)
    except TimeoutError:
        return "nothing"
    except (ConnectionError, OSError):
        return "reset"
    return f"bytes {data!r}" if data else "EOF"


def rst(writer: asyncio.StreamWriter) -> None:
    sock = writer.get_extra_info("socket")
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
    writer.transport.abort()


async def case(event: str, side: str) -> bool:
    accepted: asyncio.Queue[Stream] = asyncio.Queue()

    async def target(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        await accepted.put((reader, writer))
        await asyncio.sleep(3600)

    server = await asyncio.start_server(target, "127.0.0.1", 0)
    link = Link(1, 2, int(server.sockets[0].getsockname()[1]))
    await link.start()
    try:
        client = await asyncio.open_connection("127.0.0.1", link.port)
        client[1].write(b"hi")
        far = await asyncio.wait_for(accepted.get(), 5)
        assert await far[0].readexactly(2) == b"hi"
        link.partition()
        if event == "connect":
            await asyncio.open_connection("127.0.0.1", link.port)
            during = "a connection" if not accepted.empty() else "nothing"
            await asyncio.sleep(QUIET)
            during = "a connection" if not accepted.empty() else during
            link.heal()
            after = "a connection" if await asyncio.wait_for(accepted.get(), 5) else "nothing"
            expected = "a connection"
        else:
            actor, observer = (client, far) if side == "client" else (far, client)
            if event == "bytes":
                actor[1].write(b"held")
                await actor[1].drain()
                expected = "bytes b'held'"
            elif event == "FIN":
                actor[1].write_eof()
                expected = "EOF"
            else:
                rst(actor[1])
                expected = "reset or EOF"
            during = await seen(observer[0], QUIET)
            link.heal()
            after = await seen(observer[0], 5)
            if event == "reset" and after in ("reset", "EOF"):
                after = expected
        ok = during == "nothing" and after == expected
        label = event if event == "connect" else f"{side} {event}"
        print(f"  {'ok:  ' if ok else 'FAIL:'} {label}: partitioned, the other side sees {during}; healed, {after}")
        return ok
    finally:
        await link.close()
        server.close()


async def main() -> int:
    results = [await case(event, side) for event in ("bytes", "FIN", "reset") for side in ("client", "target")]
    results.append(await case("connect", "client"))
    failures = results.count(False)
    print(f"=== VERDICT: {'GREEN' if not failures else f'RED ({failures} failures)'}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
