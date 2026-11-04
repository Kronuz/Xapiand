#!/usr/bin/env python3
"""closing_queries.py -- the closing benchmark's neutral query sets (docs/design/closing-benchmark.md, D3 and D7).

  closing_queries.py wiki --source queries.txt --out wiki.jsonl
      C-wiki: 40 queries of each class from search-benchmark-game's queries.txt, drawn with seed 7 as prospect's
      bench/sample_queries.py does (intersection -> and, union -> or, phrase), plus a term class: queries.txt has no
      single-token query, so the 40 terms are drawn from the vocabulary of its queries.
  closing_queries.py logs --corpus logs.ndjson --out logs.jsonl
      C-logs: templates after Rally's http_logs operations, whose bounds come from the corpus: 40 each of range on
      @timestamp, range on size, a status term with a time range, sort on @timestamp and on size (half with a status
      filter), and 10 each of a date_histogram by hour, a terms on status and stats on size, each over a time window.

One JSON line per query or template (systems.py, Query.parse). The range bounds of a template are shifted per run by
Query.vary. The same seed and inputs give the same file.
"""

from __future__ import annotations

import argparse
import bz2
import json
import random
import re
import sys
from collections import Counter, defaultdict
from typing import Any

SEED = 7
PER_CLASS = 40
PER_AGG = 10
TOKEN = re.compile(r"[a-z]+")
HOUR = 3600
WIDTHS = (900, HOUR, 6 * HOUR, 24 * HOUR)  # seconds of a time window
AGG_WIDTHS = (7 * 24 * HOUR, 30 * 24 * HOUR)


def wiki_queries(source: str, per: int = PER_CLASS, seed: int = SEED) -> list[dict[str, Any]]:
    """The C-wiki set: `per` queries of each class, a class's queries drawn in file order with a seeded sample."""
    by_class: dict[str, list[list[str]]] = defaultdict(list)
    vocabulary: set[str] = set()
    with open(source) as fh:
        for number, line in enumerate(fh, 1):
            if not line.strip():
                continue
            try:
                raw = json.loads(line)
                tags = [t for t in raw["tags"] if ":" not in t]
                tokens = TOKEN.findall(raw["query"].lower())
            except (KeyError, TypeError, AttributeError) as e:
                raise ValueError(f"{source}:{number}: malformed query line: {e!r}") from e
            vocabulary.update(tokens)
            if len(tokens) >= 2 and tags and tags[0] in ("intersection", "union", "phrase"):
                by_class[tags[0]].append(tokens)
    rng = random.Random(seed)
    out: list[dict[str, Any]] = []
    terms = sorted(vocabulary)
    for token in rng.sample(terms, per):
        out.append({"class": "term", "field": "text", "terms": [token]})
    for tag, name, mode in (("intersection", "and", "all"), ("union", "or", "any"), ("phrase", "phrase", "phrase")):
        pool = by_class[tag]
        for tokens in pool if len(pool) <= per else rng.sample(pool, per):
            out.append({"class": name, "field": "text", "terms": tokens, "mode": mode})
    return out


def scan_logs(corpus: str) -> tuple[int, int, list[int], Counter[int]]:
    """The time range, a sample of sizes (every 997th document) and the status counts of the corpus."""
    opener = bz2.open if corpus.endswith(".bz2") else open
    tmin, tmax = sys.maxsize, 0
    sizes: list[int] = []
    status: Counter[int] = Counter()
    with opener(corpus, "rt") as fh:
        for n, line in enumerate(fh):
            if not line.strip():
                continue
            doc = json.loads(line)
            ts = int(doc["@timestamp"])
            tmin, tmax = min(tmin, ts), max(tmax, ts)
            status[int(doc["status"])] += 1
            if n % 997 == 0:
                sizes.append(int(doc["size"]))
    sizes.sort()
    return tmin, tmax, sizes, status


def window(rng: random.Random, tmin: int, tmax: int, widths: tuple[int, ...]) -> dict[str, Any]:
    width = rng.choice(widths)
    low = rng.randrange(tmin, max(tmin + 1, tmax - width))
    return {"field": "@timestamp", "gte": low, "lte": low + width}


def logs_queries(
    tmin: int,
    tmax: int,
    sizes: list[int],
    status: Counter[int],
    per: int = PER_CLASS,
    per_agg: int = PER_AGG,
    seed: int = SEED,
) -> list[dict[str, Any]]:
    rng = random.Random(seed)
    common = [str(s) for s, _ in status.most_common(5)]
    out: list[dict[str, Any]] = []
    for _ in range(per):
        out.append({"class": "range_time", "range": window(rng, tmin, tmax, WIDTHS)})
    for _ in range(per):
        a, b = sorted(rng.sample(range(len(sizes)), 2))
        out.append(
            {"class": "range_size", "range": {"field": "size", "gte": sizes[a], "lte": max(sizes[b], sizes[a] + 100)}}
        )
    for _ in range(per):
        out.append(
            {
                "class": "term_range",
                "field": "status",
                "terms": [rng.choice(common)],
                "range": window(rng, tmin, tmax, WIDTHS),
            }
        )
    for name, sort_field, order in (("sort_time", "@timestamp", "desc"), ("sort_size", "size", "asc")):
        for i in range(per):
            q: dict[str, Any] = {"class": name, "sort": {"field": sort_field, "order": order}}
            if i % 2 == 0:
                q.update({"field": "status", "terms": [rng.choice(common)]})
            else:
                q["range"] = window(rng, tmin, tmax, AGG_WIDTHS)
            out.append(q)
    for name, agg in (
        ("agg_histogram", {"kind": "date_histogram", "field": "@timestamp", "interval": HOUR}),
        ("agg_terms", {"kind": "terms", "field": "status"}),
        ("agg_stats", {"kind": "stats", "field": "size"}),
    ):
        for _ in range(per_agg):
            out.append({"class": name, "agg": agg, "range": window(rng, tmin, tmax, AGG_WIDTHS)})
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="set", required=True)
    w = sub.add_parser("wiki")
    w.add_argument("--source", required=True, help="search-benchmark-game's queries.txt")
    lg = sub.add_parser("logs")
    lg.add_argument("--corpus", required=True, help="the C-logs ndjson (or .bz2)")
    for p in (w, lg):
        p.add_argument("--out", required=True)
        p.add_argument("--per-class", type=int, default=PER_CLASS)
    args = ap.parse_args()
    if args.set == "wiki":
        queries = wiki_queries(args.source, args.per_class)
    else:
        tmin, tmax, sizes, status = scan_logs(args.corpus)
        queries = logs_queries(tmin, tmax, sizes, status, args.per_class, max(1, args.per_class // 4))
    with open(args.out, "w") as fh:
        fh.writelines(json.dumps(q) + "\n" for q in queries)
    counts = Counter(q["class"] for q in queries)
    print(f"{len(queries)} queries: {dict(sorted(counts.items()))}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
