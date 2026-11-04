#!/usr/bin/env python3
"""The C-wiki transform: search-benchmark-game's corpus_transform.py (its `make corpus`), reading wiki-articles.json
lines on stdin and writing {"id": url, "text": lowercase letters-only body, "sort_field": random 32-bit} lines."""

from __future__ import annotations

import json
import random
import re
import sys

PTN = re.compile("[^a-zA-Z]+")


def main() -> None:
    random.seed(42)
    out = sys.stdout
    for line in sys.stdin:
        try:
            doc = json.loads(line)
        except ValueError:
            continue
        if doc["url"] == "":
            continue
        out.write(
            json.dumps({"id": doc["url"], "text": PTN.sub(" ", doc["body"].lower()), "sort_field": random.randint(0, 2**32 - 1)})
            + "\n"
        )


if __name__ == "__main__":
    main()
