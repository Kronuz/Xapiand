#!/usr/bin/env python3
"""vocab_diff.py PORT OUT.json VECTORS_DIR: index each language's snowball sample vocabulary
(Kronuz/snowball test/vectors) and dump the INFO terms. Run against two builds and diff the files."""
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
        text = " ".join(w.capitalize() if upper and i % 3 == 0 else w for i, w in enumerate(words))
        f = {"_type": "text", "_language": code, "_stem_strategy": stem, "_stop_strategy": stop, "_positions": False, "_value": text}
        for attempt in range(2):
            s, b = req("PUT", f"/voc_{code}_{variant}/1?commit=true", {"t": f})
            if s < 300: break
        if s >= 300:
            res[f"{code}/{variant}"] = ["PUT", s, str(b)[:100]]; continue
        s, b = req("INFO", f"/voc_{code}_{variant}/1")
        res[f"{code}/{variant}"] = b.get("terms") if s < 300 else ["INFO", s]
json.dump(res, open(out, "w"), sort_keys=True, ensure_ascii=False)
print({k: (len(json.dumps(v))) for k, v in res.items()})
