#!/usr/bin/env bash
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

# power_loss_check.sh -- no acknowledged ack=quorum write is lost to a power cut on a single node.
#
#   harness/power_loss_check.sh --bin LINUX_XAPIAND [--seed S] [--writes 20] [--entries 20] [--out DIR]
#
# One run (D14's power-loss tier, docs/design/cluster-release.md): one Linux guest of the cluster VM tier
# (tools/vm/cluster.sh run --nodes 1) whose data directory is a dm-log-writes device. In the guest:
#   1. Start the node on it and PUT WRITES documents at ack=quorum, no ?commit, marking the log just before
#      each request (b<i>) and just after its answer (a<i>); seeded pauses let the autocommit's checkpoint land
#      between some of them.
#   2. kill -9 the node and stop the log: the power cut.
#   3. Replay the log to each mark and to ENTRIES random entries (60 replays at the defaults). Each replay's
#      data directory is copied out and a node recovers it; the documents it reads back at read=linearizable
#      must be a prefix 1..m of the writes, with m at least the writes answered before the replayed point.
# Exit 0 when every replay holds every acknowledged write; summary.json in --out (default build/power-loss/S).
# One run takes about 10 minutes; R2's tier is 20 runs of distinct seeds, each its own job under a host hold.
# LINUX_XAPIAND is a Linux build of xapiand the guest (Ubuntu cloud image) can run as it is.
set -u

REPO="$(cd "$(dirname "$0")/.." && pwd)"
. "$REPO/harness/env.sh"

BIN_LINUX="${BIN_LINUX:-}"
SEED="${SEED:-$RANDOM}"
WRITES=20
ENTRIES=20
OUT=""
INSIDE=0
while [ $# -gt 0 ]; do
	case $1 in
		--bin) BIN_LINUX=$2; shift 2 ;;
		--seed) SEED=$2; shift 2 ;;
		--writes) WRITES=$2; shift 2 ;;
		--entries) ENTRIES=$2; shift 2 ;;
		--out) OUT=$2; shift 2 ;;
		--inside) INSIDE=1; shift ;;
		*) sed -n '22,38p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
	esac
done
[ -n "$BIN_LINUX" ] && [ -f "$BIN_LINUX" ] || { echo "power_loss_check: --bin names no Linux xapiand" >&2; exit 2; }
OUT="${OUT:-$REPO/build/power-loss/$SEED}"

if [ "$INSIDE" = 0 ]; then
	mkdir -p "$OUT"
	exec "$REPO/tools/vm/cluster.sh" run --nodes 1 --out "$OUT/vm" -- \
		"$0" --inside --bin "$BIN_LINUX" --seed "$SEED" --writes "$WRITES" --entries "$ENTRIES" --out "$OUT"
fi

guest() { "$REPO/tools/vm/cluster.sh" ssh 1 "$@"; }
guest 'cat > ~/xapiand && chmod +x ~/xapiand' <"$BIN_LINUX" || { echo "power_loss_check: cannot copy the binary" >&2; exit 2; }
guest "python3 -I - --seed $SEED --writes $WRITES --entries $ENTRIES" >"$OUT/summary.json" <<'PY'
import argparse, json, os, random, re, shutil, subprocess, sys, time, urllib.error, urllib.request
sys.path.insert(0, "/opt/cluster/guest")
from disk import ask  # the disk server's mark, stop, replay and unreplay

args = argparse.ArgumentParser()
args.add_argument("--seed", type=int)
args.add_argument("--writes", type=int)
args.add_argument("--entries", type=int)
args = args.parse_args()
rng = random.Random(args.seed)
BIN, PORT, INDEX = os.path.expanduser("~/xapiand"), 8880, "powerloss"


def http(method, path, body=None):
    request = urllib.request.Request(f"http://127.0.0.1:{PORT}{path}", method=method, data=body,
                                     headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(request, timeout=30) as reply:
            return reply.status
    except urllib.error.HTTPError as error:
        return error.code


def start(data):
    node = subprocess.Popen([BIN, "--port", str(PORT), "--name", "powerloss", "--verbosity", "3", "-D", data],
                            stdout=subprocess.DEVNULL, stderr=open(os.path.expanduser("~/node.log"), "ab"))
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        if node.poll() is not None:
            raise RuntimeError(f"the node exited {node.returncode} at start on {data}")
        try:
            http("GET", "/")
            return node
        except OSError:
            time.sleep(0.2)
    node.kill()
    raise RuntimeError(f"the node never answered on {data}")


def kill(node):
    node.kill()
    node.wait()


data = str(ask("ping")["mnt"])
node = start(data)
for i in range(1, args.writes + 1):
    ask("mark", f"b{i}")
    status = http("PUT", f"/{INDEX}/{i}", json.dumps({"i": i}).encode())
    if status != 204:
        raise SystemExit(f"write {i} answered {status}")
    ask("mark", f"a{i}")
    time.sleep(rng.choice((0, 0, 0.05, 1.5)))  # 1.5 s lets the autocommit's checkpoint run
kill(node)
stopped = ask("stop")
marks = {}
for point in ask("points")["points"]:
    found = re.match(r"#(\d+) mark ([ab])(\d+)$", point)
    if found:
        marks[(found[2], int(found[3]))] = int(found[1])
targets = [(f"{kind}{i}", i - 1 if kind == "b" else i) for i in range(1, args.writes + 1) for kind in "ba"]
for entry in sorted(rng.sample(range(stopped["entries"]), min(args.entries, stopped["entries"]))):
    targets.append((entry, sum(1 for (kind, i), at in marks.items() if kind == "a" and at <= entry)))

copy, results, lost = os.path.expanduser("~/replayed"), [], 0
for to, acknowledged in targets:
    started = time.monotonic()
    replay = ask("replay", str(to))["replay"]
    shutil.rmtree(copy, ignore_errors=True)
    subprocess.run(["sudo", "cp", "-a", replay, copy], check=True)
    subprocess.run(["sudo", "chown", "-R", f"{os.getuid()}:{os.getgid()}", copy], check=True)
    ask("unreplay")
    node = start(copy)
    present = [i for i in range(1, args.writes + 1)
               if http("GET", f"/{INDEX}/{i}?read=linearizable") == 200]
    kill(node)
    prefix = present == list(range(1, len(present) + 1))
    passed = prefix and len(present) >= acknowledged
    lost += not passed
    results.append({"to": to, "acknowledged": acknowledged, "present": present, "passed": passed,
                    "seconds": round(time.monotonic() - started, 3)})
    print(f"replay {to}: acknowledged {acknowledged}, present {present}, {'ok' if passed else 'LOST'}",
          file=sys.stderr, flush=True)
seconds = sorted(result["seconds"] for result in results)
print(json.dumps({"seed": args.seed, "writes": args.writes, "log": stopped, "replays": len(results),
                  "failed": lost, "seconds_per_replay": {"median": seconds[len(seconds) // 2], "max": seconds[-1]},
                  "results": results}, indent=1))
sys.exit(1 if lost else 0)
PY
status=$?
echo "power_loss_check seed $SEED: $([ $status = 0 ] && echo PASS || echo FAIL) ($OUT/summary.json)"
exit $status
