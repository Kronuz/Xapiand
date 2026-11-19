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

"""vm.py's sweep of a run whose `up` died before its guardian wrote state.json, on any host, no VM.

    python3 sweep_test.py

Such a run keeps its overlays until its owner is dead; a live owner's run, and a run with no owner
file yet (`up` is between mkdir and writing it), are left alone.
"""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import vm


def main() -> int:
    failed = False
    with tempfile.TemporaryDirectory(dir=Path.home() / ".scratch") as tmp:
        root = Path(tmp)
        dead = vm.os.getpid()
        while vm.alive(dead):
            dead += 1  # a pid that no process has
        for name, owner in (("run-dead", str(dead)), ("run-live", str(vm.os.getpid())), ("run-fresh", None)):
            (root / name).mkdir()
            (root / name / "node1.qcow2").write_text("")
            if owner:
                (root / name / "owner").write_text(owner + "\n")
        swept = [path.name for path in vm.sweep(root)]
        for name, gone in (("run-dead", True), ("run-live", False), ("run-fresh", False)):
            passed = (root / name / "node1.qcow2").exists() != gone and (name in swept) == gone
            failed |= not passed
            print(f"{'ok  ' if passed else 'FAIL'} sweep {name}: overlays {'deleted' if gone else 'kept'}")
    print(f"sweep_test: {'FAIL' if failed else 'PASS'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
