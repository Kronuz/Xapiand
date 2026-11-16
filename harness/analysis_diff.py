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

"""analysis_diff.py PORT OUT.json: index a text corpus under many analysis configs and dump what
searches find. Per config and per probe (each word of the corpus, and each pair of adjacent words as a
phrase) it records the ids of the documents that match. Run against two builds (or engines) on separate
data dirs and diff the files: a difference is a change in how text is analysed, at index or query time."""
import json, re, sys, urllib.request, urllib.error, itertools

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

WORDS = [re.findall(r"\w+", text) for text in CORPUS]
PROBES = sorted({w for words in WORDS for w in words} | {f"\"{a} {b}\"" for words in WORDS for a, b in zip(words, words[1:])})

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
            res[f"doc{i}"] = ["PUT", s, (b or {}).get("message") if isinstance(b, dict) else b]
    for probe in PROBES:
        s, b = req("SEARCH", f"{idx}", {"_query": {"t": probe}, "_limit": 100})
        res[probe] = sorted(h["_id"] for h in b["hits"]) if s < 300 and isinstance(b, dict) and "hits" in b else ["SEARCH", s]
    results[key] = res
json.dump(results, open(out, "w"), indent=1, sort_keys=True, ensure_ascii=False)
print("configs", n)
