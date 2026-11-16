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

"""strata_write_model.py -- a client with a model of what the node must serve, for strata_write_check.sh.

Every document is written as {"seq": k} where k is the version the write must produce, so a served
document is right when its _version equals its seq (a replay that applies a write twice, or rolls one
back, breaks that) and equals what was acknowledged. The model (a JSON file) keeps, per id, the last
acknowledged version (None: deleted) and the one write that was sent but not answered when the node died.

  put    PORT INDEX MODEL ID...      one PUT each, version = model + 1
  del    PORT INDEX MODEL ID...      one DELETE each
  load   PORT INDEX MODEL SECONDS N  random PUT/DELETE over ids u0..u{N-1} until the time is up or the node stops answering
  verify PORT INDEX MODEL            every id in the model is served at its acknowledged version (or, for the
                                     one unanswered write, at that version or the one before it)
"""
import json
import random
import sys
import time
import urllib.error
import urllib.request


def request(method, url, body=None, timeout=10):
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(url, data=data, method=method, headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as response:
            raw = response.read()
            return response.status, (json.loads(raw) if raw else {})
    except urllib.error.HTTPError as error:
        raw = error.read()
        try:
            return error.code, json.loads(raw)
        except ValueError:
            return error.code, {}


def load_model(path):
    try:
        with open(path) as f:
            return json.load(f)
    except FileNotFoundError:
        return {"docs": {}, "inflight": {}}


def save_model(path, model):
    with open(path, "w") as f:
        json.dump(model, f)


def write(base, model, ident, delete=False):
    """One write; the model learns the outcome only when the node answers. Returns False if it did not."""
    docs, inflight = model["docs"], model["inflight"]
    current = docs.get(ident)
    if delete:
        if current is None:
            return True
        inflight[ident] = {"op": "del", "from": current}
    else:
        inflight[ident] = {"op": "put", "from": current, "to": (current or 0) + 1}
    try:
        if delete:
            status, _ = request("DELETE", f"{base}/{ident}")
        else:
            status, _ = request("PUT", f"{base}/{ident}", {"seq": (current or 0) + 1})
    except (urllib.error.URLError, ConnectionError, TimeoutError, OSError):
        return False
    del inflight[ident]
    if status in (200, 201, 204):
        docs[ident] = None if delete else (current or 0) + 1
        return True
    print(f"  unexpected status {status} for {'DELETE' if delete else 'PUT'} {ident}")
    return True


def verify(base, model):
    """Checks every id; a write that was in flight when the node died may or may not have happened, and
    what the node serves settles it in the model."""
    bad = []
    served = 0
    for ident in sorted(set(model["docs"]) | set(model["inflight"])):
        version = model["docs"].get(ident)
        pending = model["inflight"].get(ident)
        allowed = {version}
        if pending:
            allowed.add(None if pending["op"] == "del" else pending["to"])
        try:
            status, body = request("GET", f"{base}/{ident}")
        except (urllib.error.URLError, OSError) as error:
            bad.append((ident, f"no answer: {error}"))
            continue
        if status == 404:
            got = None
        elif status == 200:
            got = body.get("_version")
            if body.get("seq") != got:
                bad.append((ident, f"seq {body.get('seq')} != _version {got}"))
                continue
        else:
            bad.append((ident, f"status {status}"))
            continue
        if got in allowed:
            served += 1
            model["docs"][ident] = got
            model["inflight"].pop(ident, None)
        else:
            bad.append((ident, f"version {got}, acknowledged {version}" + (f", in flight {pending}" if pending else "")))
    return served, bad


def main(argv):
    command, port, index, model_path = argv[1:5]
    base = f"http://localhost:{port}/{index}"
    model = load_model(model_path)
    try:
        if command in ("put", "del"):
            for ident in argv[5:]:
                if not write(base, model, ident, delete=(command == "del")):
                    break
        elif command == "load":
            seconds, count = float(argv[5]), int(argv[6])
            rng = random.Random(int(time.time()))
            end = time.time() + seconds
            while time.time() < end:
                ident = f"u{rng.randrange(count)}"
                if not write(base, model, ident, delete=(rng.random() < 0.08)):
                    break
        elif command == "verify":
            served, bad = verify(base, model)
            total = len(model["docs"])  # after the in-flight ids joined it
            print(f"verified {served} of {total} documents; {len(model['inflight'])} write(s) in flight")
            for ident, why in bad[:10]:
                print(f"  BAD {ident}: {why}")
            return 0 if not bad else 1
        else:
            print(__doc__)
            return 2
    finally:
        save_model(model_path, model)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
