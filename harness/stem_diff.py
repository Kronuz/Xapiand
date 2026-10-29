#!/usr/bin/env python3
"""stem_diff.py PORT OUT.json VECTORS_DIR: index each language's snowball sample vocabulary
(Kronuz/snowball test/vectors), one document per word, and dump for every word the documents a search
for it finds (the words that stem alike). Run against two builds (or engines) and diff the files."""
import json, sys, urllib.request, urllib.error
port, out, vec = sys.argv[1], sys.argv[2], sys.argv[3].rstrip("/") + "/"
base = f"http://localhost:{port}"
LANGS = {"ca": "catalan", "da": "danish", "nl": "dutch", "en": "english", "fi": "finnish", "fr": "french", "de": "german", "hu": "hungarian", "it": "italian", "no": "norwegian", "pt": "portuguese", "ro": "romanian", "ru": "russian", "es": "spanish", "sv": "swedish", "tr": "turkish", "hy": "armenian", "eu": "basque"}

def req(method, path, body=None):
    data = json.dumps(body).encode() if body is not None else None
    r = urllib.request.Request(base + path, data=data, method=method, headers={"Content-Type": "application/json", "Accept": "application/json"})
    try:
        with urllib.request.urlopen(r, timeout=60) as resp:
            return resp.status, json.loads((resp.read() or b"null").decode("utf-8", "replace"))
    except urllib.error.HTTPError as e:
        return e.code, json.loads((e.read() or b"null").decode("utf-8", "replace"))

res = {}
for code, name in LANGS.items():
    words = [l.split("\t")[0] for l in open(vec + name + ".tsv", encoding="utf-8").read().splitlines() if l]
    for variant, (stem, stop, upper) in {"z": ("stem_all_z", "stop_none", False), "some_up": ("stem_some", "stop_all", True)}.items():
        idx = f"/voc_{code}_{variant}/"
        for i, w in enumerate(words):
            f = {"_type": "text", "_language": code, "_stem_strategy": stem, "_stop_strategy": stop, "_positions": False, "_value": w.capitalize() if upper and i % 3 == 0 else w}
            for attempt in range(2):
                s, b = req("PUT", f"{idx}{i}", {"t": f})
                if s < 300: break
        req("POST", f"{idx}:commit")
        found = {}
        for i, w in enumerate(words):
            s, b = req("SEARCH", idx, {"_query": {"t": w}, "_limit": 1000})
            found[f"{i}:{w}"] = sorted(int(h["_id"]) for h in b["hits"]) if s < 300 and isinstance(b, dict) and "hits" in b else ["SEARCH", s]
        res[f"{code}/{variant}"] = found
json.dump(res, open(out, "w"), sort_keys=True, ensure_ascii=False)
print({k: sum(len(v) for v in d.values()) for k, d in res.items()})
