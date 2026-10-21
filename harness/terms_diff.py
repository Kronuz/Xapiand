#!/usr/bin/env python3
"""terms_diff.py PORT OUT.json: index a text corpus under many analysis configs and dump the
INFO terms. Run against two builds and diff the files; compare with e2e_diff.py/bodydiff.py."""
import json, sys, urllib.request, urllib.error, itertools

port, out = sys.argv[1], sys.argv[2]
base = f"http://localhost:{port}"

CORPUS = [
    "The quick brown fox jumps over the lazy dog.",
    "Running runners ran quickly; the Foxes' dens were NOT empty. It's O'Reilly's.",
    "Pi is 3.14159, 1,000,000 or 1.000.000; version 2.0.1 and e-mail me@example.com, http://x.org/a_b?c=d&e=f",
    "C++ and C# and U.S.A. and a.b.c. A&B AT&T 3-4 x_y_z __init__ foo@bar @user #tag",
    "Café naïve résumé Ünïcödé ÀÉÎÕÜ straße ǅ İstanbul ΑΘΗΝΑ Привет мир Тест, ТЕСТЫ",
    "日本語のテキストです。中文文本测试 한국어 텍스트 mixed English 漢字",
    "supercalifragilisticexpialidocious " + "x" * 70 + " " + "é" * 40 + " end",
    "",
    "   \t\n  ",
    "a an the and of to in is it that this these those was were be been being",
    "Le chat mange la souris; el niño come manzanas; der Hund läuft schnell; il gatto è bello",
    "word1 word2  word3\tword4\nword5 -dash- (paren) [brk] {brc} \"quote\" 'single' ‘curly’ “dq” a­b 1​2",
    "Mr. Smith's 3rd place; 10th, 21st; N.A.S.A; I.B.M. or IBM; foo.bar.baz; 100%; $5.99; 5,5; 5;5",
    "emoji 😀 text 👍🏽 ok; combining é å; zero‍width; fullwidth ＡＢＣ１２３",
    "repeat repeat repeat Repeat REPEAT stopwords the the The",
]

LANGS = [("", ""), ("en", ""), ("fr", ""), ("ru", ""), ("es", ""), ("tr", ""), ("hy", ""), ("eu", ""), ("ca", ""), ("no", ""), ("nl", ""), ("sv", ""), ("it", ""), ("pt", ""), ("ro", ""), ("hu", ""), ("fi", ""), ("da", ""), ("en", "fr"), ("", "en"), ("fr", "en"), ("de", ""), ("basque", "")]
STEMS = ["stem_none", "stem_some", "stem_all", "stem_all_z"]
STOPS = ["stop_none", "stop_all", "stop_stemmed"]

def req(method, path, body=None):
    data = json.dumps(body).encode() if body is not None else None
    r = urllib.request.Request(base + path, data=data, method=method, headers={"Content-Type": "application/json", "Accept": "application/json"})
    try:
        with urllib.request.urlopen(r, timeout=60) as resp:
            return resp.status, json.loads((resp.read() or b"null").decode("utf-8", "replace"))
    except urllib.error.HTTPError as e:
        try:
            b = json.loads((e.read() or b"null").decode("utf-8", "replace"))
        except Exception:
            b = None
        return e.code, b

results = {}
n = 0
for (lang, slang), stem, stop, positions, cjk in itertools.product(LANGS, STEMS, STOPS, [True, False], [False, True]):
    if cjk and (stem != "stem_some" or stop != "stop_stemmed" or (lang, slang) not in (("", ""), ("en", ""))):
        continue
    if not lang and not slang and (stem != "stem_some" or stop != "stop_stemmed"):
        continue
    if stem != "stem_some" and (lang, slang) not in (("en", ""), ("ru", ""), ("en", "fr"), ("", "en")):
        continue
    if stop != "stop_stemmed" and (lang, slang) not in (("en", ""), ("ru", ""), ("fr", ""), ("en", "fr"), ("fr", "en")):
        continue
    n += 1
    idx = f"/td{n}/"
    field = {"_type": "text", "_positions": positions, "_cjk_ngram": cjk, "_weight": 3 if n % 3 == 0 else 1}
    if lang or slang:
        field["_stem_strategy"] = stem
        field["_stop_strategy"] = stop
    if lang:
        field["_language"] = lang
    if slang:
        field["_stem_language"] = slang
    key = f"lang={lang}/{slang} stem={stem} stop={stop} pos={positions} cjk={cjk}"
    res = {}
    for i, text in enumerate(CORPUS, 1):
        f = dict(field); f["_value"] = text
        s, b = req("PUT", f"{idx}{i}?commit=true", {"t": f})
        if s >= 300:
            res[str(i)] = ["PUT", s, (b or {}).get("message") if isinstance(b, dict) else b]
            continue
        s, b = req("INFO", f"{idx}{i}")
        if s >= 300:
            res[str(i)] = ["INFO", s, (b or {}).get("message") if isinstance(b, dict) else b]
            continue
        res[str(i)] = b.get("terms")
    results[key] = res
json.dump(results, open(out, "w"), indent=1, sort_keys=True, ensure_ascii=False)
print("configs", n)
