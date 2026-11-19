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


"""net.py's tc and nft commands, recorded instead of run, and its commands against a model kernel.

    python3 net_test.py [NET_PY]   (default: net.py beside it; no root, no Linux)

tc reads class and handle numbers in hex, so a band written in decimal past 9 names a class the
prio qdisc never made; the kernel refuses it only in a guest, and this finds it on any host. nft
likewise wants a newline or ';' after each closing brace of a table's set or chain. A setup that
stops partway (a tc batch stops at its first error, or nft refuses the table) must not leave a
guest where a later shape or partition exits 0 and does nothing.
"""

from __future__ import annotations

import importlib.util
import re
import sys
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / ".faultlab" / "python"))


def load(net_py: Path) -> Any:
    spec = importlib.util.spec_from_file_location("net_under_test", net_py)
    assert spec is not None and spec.loader is not None
    net = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(net)
    return net


def commands(net_py: Path, argv: list[str]) -> list[str]:
    """The tc and nft command lines net.py's main(argv) runs, batch lines included, in order."""
    net = load(net_py)
    seen: list[str] = []

    def record(argv: list[str], stdin: str | None = None, check: bool = True) -> str:
        if argv[:2] == ["nft", "-f"] and stdin is not None:
            seen.extend(f"nft-file {line}" for line in stdin.splitlines())
        elif argv[:2] == ["tc", "-batch"] and stdin is not None:
            seen.extend(stdin.splitlines())
        elif argv[:2] != ["tc", "qdisc"] or argv[2] != "show":
            seen.append(" ".join(argv[1:] if argv[0] == "tc" else argv))
        return ""

    setattr(net, "run", record)  # noqa: B010 -- a module loaded from a path has no declared attributes
    net.main(argv)
    return seen


def check(lines: list[str]) -> list[str]:
    """What is wrong with a command sequence: each class it names beyond the prio's bands, and each
    nft block closed with something after it on the same line, which nft's parser refuses."""
    errors: list[str] = []
    bands = 0
    for line in lines:
        if m := re.search(r"root handle 1: prio bands (\d+)", line):
            bands = int(m[1])
        for word in re.findall(r"(?:parent|flowid) 1:([0-9a-f]+)\b", line):
            if not 1 <= int(word, 16) <= bands:
                errors.append(f"class 1:{word} is not one of the {bands} bands: {line}")
        if line.startswith("nft-file ") and re.search(r"\}\s*[^\s;}]", line):
            errors.append(f"nft needs a newline or ';' after a block's closing brace: {line}")
    return errors


class Kernel:
    """The tc and nft state a guest keeps, as far as net.py's commands change it, with injectable faults:
    `tc_lines` is how many lines of a tc batch apply before the next fails, `nft_refuses` makes nft
    refuse the table."""

    def __init__(self) -> None:
        self.prio = False
        self.filters: set[int] = set()
        self.leaves: dict[int, str] = {}
        self.table = False
        self.cut: set[str] = set()
        self.tc_lines: int | None = None
        self.nft_refuses = False

    def fail(self, argv: list[str], why: str) -> str:
        raise SystemExit(f"model kernel: {' '.join(argv)}: {why}")

    def tc_line(self, line: str) -> None:
        if m := re.match(r"qdisc replace dev \S+ root handle 1: prio", line):
            self.prio = True
        elif m := re.match(r"qdisc replace dev \S+ parent 1:([0-9a-f]+) handle \S+ netem(.*)", line):
            if not self.prio:
                self.fail([line], "no root qdisc")
            self.leaves[int(m[1], 16) - 1] = m[2].strip()
        elif m := re.search(r"match ip dst 10\.87\.0\.(\d+)/32", line):
            if not self.prio:
                self.fail([line], "no root qdisc")
            self.filters.add(int(m[1]))

    def run(self, argv: list[str], stdin: str | None = None, check: bool = True) -> str:
        if argv[:3] == ["tc", "qdisc", "show"]:
            return " prio 1: " if self.prio else ""
        if argv[:2] == ["tc", "-batch"]:
            for number, line in enumerate((stdin or "").splitlines()):
                if self.tc_lines is not None and number >= self.tc_lines:
                    return self.fail(argv, f"line {number + 1} refused")
                self.tc_line(line)
        elif argv[:3] == ["tc", "qdisc", "replace"]:
            self.tc_line(" ".join(argv[1:]))
        elif argv[:2] == ["nft", "-f"] and "table inet" in (stdin or ""):
            if self.nft_refuses:
                return self.fail(argv, "refused")
            self.table = True
        else:
            self.other(argv, stdin)
        return ""

    def other(self, argv: list[str], stdin: str | None) -> None:
        text = stdin or " ".join(argv[1:])
        if m := re.search(r"(add|delete) element inet cluster cut \{ (\S+) \}", text):
            if not self.table:
                self.fail(argv, "no such table")
            if m[1] == "add":
                self.cut.add(m[2])
            elif m[2] in self.cut:
                self.cut.remove(m[2])
            else:
                self.fail(argv, "no such element")
        elif "flush set inet cluster cut" in text:
            self.cut.clear()


def applied(kernel: Kernel, argv: list[str]) -> bool:
    """Whether main(argv) having exited 0 left the effect it names in the kernel."""
    if argv[0] == "shape":
        peer = int(argv[1])
        return kernel.prio and peer in kernel.filters and "delay" in kernel.leaves.get(peer, "")
    return kernel.table and f"10.87.0.{argv[1]}" in kernel.cut


def interrupted(net_py: Path) -> list[str]:
    """A setup stopped partway, then the same command with the fault gone: it must exit 0 and apply its
    effect, never exit 0 doing nothing. Returns what went wrong."""
    errors: list[str] = []
    shape, cut = ["shape", "3", "--ms", "50"], ["partition", "2"]
    cases: list[tuple[str, list[str], Any]] = [
        ("a tc batch that stops after 5 lines", shape, lambda k: setattr(k, "tc_lines", 5)),
        ("nft refusing the table", cut, lambda k: setattr(k, "nft_refuses", True)),
        ("a prio root and nothing else", shape, lambda k: setattr(k, "prio", True)),
        ("a prio root and nothing else", cut, lambda k: setattr(k, "prio", True)),
    ]
    for what, argv, fault in cases:
        kernel = Kernel()
        fault(kernel)
        net = load(net_py)
        net.run = kernel.run
        try:
            net.main(argv)
        except SystemExit:
            pass  # a setup that fails says so
        kernel.tc_lines = None
        kernel.nft_refuses = False
        try:
            net.main(argv)
        except SystemExit as stop:
            errors.append(f"after {what}, `{' '.join(argv)}` fails again: {stop}")
            continue
        if not applied(kernel, argv):
            errors.append(f"after {what}, `{' '.join(argv)}` exits 0 and applies nothing")
    return errors


def main(argv: list[str]) -> int:
    net_py = Path(argv[0]) if argv else HERE / "net.py"
    failed = False
    errors = interrupted(net_py)
    failed |= bool(errors)
    print(
        f"{'FAIL' if errors else 'ok  '} net.py after a setup stopped partway{''.join(chr(10) + '     ' + e for e in errors)}"
    )
    for case in (["shape", "2", "--ms", "50"], ["shape", "15", "--loss", "0.1"], ["partition", "3"]):
        errors = check(commands(net_py, case))
        failed |= bool(errors)
        print(f"{'FAIL' if errors else 'ok  '} net.py {' '.join(case)}{''.join(chr(10) + '     ' + e for e in errors)}")
    print(f"net_test: {'FAIL' if failed else 'PASS'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
