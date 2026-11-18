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

"""N `xapiand` processes on 127.0.0.1, each with its own ports and data directory, and their faults.

Every node gets an HTTP port and a peer port from the OS, so concurrent runs never share one, and
node i dials node j through the mesh's link (i, j) (faultlab/proxy.py), which partitions, resets and caps.
The harness's process group owns every node: run.sh kills the group on exit and on timeout.
"""

from __future__ import annotations

import asyncio
import contextlib
import http.client
import json
import os
import shlex
import signal
import socket
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from faultlab.proxy import FaultLog, Mesh

READY_SECONDS = 60.0


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


@dataclass
class Response:
    status: int  # 0 when no response arrived
    body: Any = None  # the decoded JSON, or None
    error: str = ""


def _request(port: int, method: str, path: str, body: object, timeout: float) -> Response:
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        payload = None if body is None else json.dumps(body)
        headers = {"Content-Type": "application/json", "Accept": "application/json"}
        connection.request(method, path, payload, headers)
        response = connection.getresponse()
        data = response.read()
        try:
            decoded = json.loads(data) if data else None
        except ValueError:
            decoded = None
        return Response(response.status, decoded)
    except (OSError, http.client.HTTPException) as error:
        return Response(0, None, f"{type(error).__name__}: {error}")
    finally:
        connection.close()


async def request(port: int, method: str, path: str, body: object = None, timeout: float = 30.0) -> Response:
    return await asyncio.to_thread(_request, port, method, path, body, timeout)


@dataclass
class View:
    """What one node says about the cluster: its members and its leader."""

    name: str
    members: frozenset[str]
    leader: str | None


def parse_view(body: object) -> View | None:
    """Reads `GET /`. A node without a `nodes` list is a cluster of one that it leads."""
    name = body.get("name") if isinstance(body, dict) else None
    if not isinstance(body, dict) or not isinstance(name, str):
        return None
    nodes = body.get("nodes")
    if not isinstance(nodes, list):
        return View(name, frozenset({name}), name)
    members = frozenset(str(n["name"]) for n in nodes if isinstance(n, dict) and "name" in n)
    leaders = [str(n["name"]) for n in nodes if isinstance(n, dict) and n.get("leader")]
    return View(name, members, leaders[0] if len(leaders) == 1 else None)


@dataclass
class Node:
    index: int
    name: str
    http: int
    peer: int
    root: Path
    process: asyncio.subprocess.Process | None = None
    launches: int = 0
    paused: bool = False
    exits: list[int] = field(default_factory=list)

    @property
    def data(self) -> Path:
        return self.root / "data"

    @property
    def log(self) -> Path:
        return self.root / "node.log"

    @property
    def alive(self) -> bool:
        return self.process is not None and self.process.returncode is None


class Cluster:
    """Starts, faults and stops the nodes. `node_args` applies to every node, `join_args` to all but node 1.

    Both are templates over {index} {name} {http} {peer} {data} {peers} {seed}: {peers} lists the
    link addresses this node dials the others by, and {seed} is its link to node 1.
    """

    def __init__(
        self,
        binary: Path,
        root: Path,
        count: int,
        node_args: str = "",
        join_args: str = "",
        log: FaultLog | None = None,
    ) -> None:
        self.binary = binary
        self.root = root
        self.node_args = node_args
        self.join_args = join_args
        self.mesh = Mesh(log)
        self.nodes = {i: Node(i, f"node{i}", free_port(), free_port(), root / f"node{i}") for i in range(1, count + 1)}

    def __len__(self) -> int:
        return len(self.nodes)

    def by_name(self, name: str) -> Node | None:
        return next((node for node in self.nodes.values() if node.name == name), None)

    def arguments(self, node: Node) -> list[str]:
        others = [j for j in self.nodes if j != node.index]
        fields = {
            "index": str(node.index),
            "name": node.name,
            "http": str(node.http),
            "peer": str(node.peer),
            "data": str(node.data),
            "peers": ",".join(self.mesh.address(node.index, j) for j in others),
            "seed": self.mesh.address(node.index, 1) if node.index != 1 else "",
        }
        template = self.node_args + (" " + self.join_args if node.index != 1 else "")
        extra = [part.format_map(fields) for part in shlex.split(template)]
        return ["--port", str(node.http), "--name", node.name, "-D", str(node.data), *extra]

    async def start(self) -> list[str]:
        """Launches node 1, waits for it, then the rest; returns the nodes that never answered."""
        await self.mesh.start({i: node.peer for i, node in self.nodes.items()})
        missing: list[str] = []
        for i, node in self.nodes.items():
            await self.launch(node)
            if i == 1 and not await self.ready(node):
                missing.append(node.name)
        for node in list(self.nodes.values())[1:]:
            if not await self.ready(node):
                missing.append(node.name)
        return missing

    async def launch(self, node: Node) -> None:
        node.root.mkdir(parents=True, exist_ok=True)
        node.launches += 1
        with node.log.open("ab") as log:
            log.write(f"\n=== launch {node.launches}: {shlex.join(self.arguments(node))}\n".encode())
            node.process = await asyncio.create_subprocess_exec(
                str(self.binary), *self.arguments(node), stdin=asyncio.subprocess.DEVNULL, stdout=log, stderr=log
            )
        node.paused = False

    async def ready(self, node: Node, seconds: float = READY_SECONDS) -> bool:
        """Waits until the node answers `GET /` with 200; false when it exits or the time runs out."""
        loop = asyncio.get_running_loop()
        deadline = loop.time() + seconds
        while loop.time() < deadline:
            if not node.alive:
                return False
            if (await request(node.http, "GET", "/", timeout=2)).status == 200:
                return True
            await asyncio.sleep(0.25)
        return False

    async def view(self, node: Node) -> View | None:
        if not node.alive:
            return None
        response = await request(node.http, "GET", "/", timeout=5)
        return parse_view(response.body) if response.status == 200 else None

    async def kill9(self, node: Node) -> None:
        await self._signal(node, signal.SIGKILL)

    def pause(self, node: Node) -> None:
        if node.alive and node.process is not None:
            node.process.send_signal(signal.SIGSTOP)
            node.paused = True

    def resume(self, node: Node) -> None:
        if node.alive and node.process is not None:
            node.process.send_signal(signal.SIGCONT)
            node.paused = False

    async def stop(self, node: Node, grace: float = 10.0) -> None:
        """SIGTERM, then SIGKILL after `grace` seconds."""
        await self._signal(node, signal.SIGTERM, grace)

    async def restart(self, node: Node) -> bool:
        if node.alive:
            await self.stop(node)
        await self.launch(node)
        return await self.ready(node)

    async def _signal(self, node: Node, signum: int, grace: float = 0.0) -> None:
        process = node.process
        if process is None or process.returncode is not None:
            return
        with contextlib.suppress(ProcessLookupError):
            if node.paused:
                process.send_signal(signal.SIGCONT)
            process.send_signal(signum)
        try:
            await asyncio.wait_for(process.wait(), grace or 30)
        except TimeoutError:
            with contextlib.suppress(ProcessLookupError):
                process.kill()
            await process.wait()
        node.paused = False
        node.exits.append(process.returncode if process.returncode is not None else -1)

    async def close(self) -> None:
        await asyncio.gather(*(self.stop(node) for node in self.nodes.values()), return_exceptions=True)
        await self.mesh.close()

    def kill_now(self) -> None:
        """For signal handlers and atexit: SIGKILL every node without awaiting."""
        for node in self.nodes.values():
            if node.alive and node.process is not None:
                with contextlib.suppress(ProcessLookupError):
                    os.kill(node.process.pid, signal.SIGKILL)
