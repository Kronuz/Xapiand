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


"""net.py's tc and nft commands, recorded instead of run: every class a qdisc or filter names exists.

    python3 net_test.py [NET_PY]   (default: net.py beside it; no root, no Linux)

tc reads class and handle numbers in hex, so a band written in decimal past 9 names a class the
prio qdisc never made; the kernel refuses it only in a guest, and this finds it on any host.
"""

from __future__ import annotations

import importlib.util
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / ".faultlab" / "python"))


def commands(net_py: Path, argv: list[str]) -> list[str]:
    """The tc and nft command lines net.py's main(argv) runs, batch lines included, in order."""
    spec = importlib.util.spec_from_file_location("net_under_test", net_py)
    assert spec is not None and spec.loader is not None
    net = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(net)
    seen: list[str] = []

    def record(argv: list[str], stdin: str | None = None) -> str:
        if argv[:2] == ["tc", "-batch"] and stdin is not None:
            seen.extend(stdin.splitlines())
        elif argv[:2] != ["tc", "qdisc"] or argv[2] != "show":
            seen.append(" ".join(argv[1:] if argv[0] == "tc" else argv))
        return ""

    setattr(net, "run", record)  # noqa: B010 -- a module loaded from a path has no declared attributes
    net.main(argv)
    return seen


def check(lines: list[str]) -> list[str]:
    """What is wrong with a command sequence: each class it names beyond the prio's bands."""
    errors: list[str] = []
    bands = 0
    for line in lines:
        if m := re.search(r"root handle 1: prio bands (\d+)", line):
            bands = int(m[1])
        for word in re.findall(r"(?:parent|flowid) 1:([0-9a-f]+)\b", line):
            if not 1 <= int(word, 16) <= bands:
                errors.append(f"class 1:{word} is not one of the {bands} bands: {line}")
    return errors


def main(argv: list[str]) -> int:
    net_py = Path(argv[0]) if argv else HERE / "net.py"
    failed = False
    for case in (["shape", "2", "--ms", "50"], ["shape", "15", "--loss", "0.1"], ["partition", "3"]):
        errors = check(commands(net_py, case))
        failed |= bool(errors)
        print(f"{'FAIL' if errors else 'ok  '} net.py {' '.join(case)}{''.join(chr(10) + '     ' + e for e in errors)}")
    print(f"net_test: {'FAIL' if failed else 'PASS'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
