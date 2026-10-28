#!/usr/bin/env bash
#
# cluster_check.sh [N]   -- the multi-node correctness net (the missing counterpart to
# e2e_check.sh, which only runs a single --solo node so Discovery / Remote / Replication
# are NEVER exercised by it).
#
# Spins an N-node (default 2, try 3) Xapiand cluster on localhost and asserts the three
# things the --solo E2E can't: the nodes DISCOVER each other and agree on membership, a
# write on one node is READABLE from the others (shards distributed across nodes, reached
# over the Remote protocol / replicated), and a distributed SEARCH gathers hits across
# every node's shards.
#
# This is the prerequisite net for migrating Remote / Replication / Discovery off libev
# onto the Asio runtime (Kronuz/reactor): those refactors are otherwise BUILD-validated
# only. Green here = the cluster data plane still works end to end.
#
# The recipe + its gotchas (learned the hard way):
#   * Nodes share ONE discovery port (58880, multicast group 239.192.168.1, IP_MULTICAST_LOOP
#     so packets loop back on a single host) but each needs its OWN http / xapian(remote) /
#     replica(replication) TCP ports -- two nodes can't bind the same TCP port. Each node
#     gossips its own ports, so the others learn them.
#   * Cluster setup is SLOW to settle (~8s/node: discovery + Raft-ish setup). The FIRST
#     write to a fresh cluster is slower still (~10-20s: shards are created across nodes on
#     first touch, "Checked out shard is taking too long"). So the timeouts here are large
#     on purpose -- a fast timeout looks like a failure when it's just first-touch latency.
#   * Readiness = GET / returns 200 AND the node's own nodes[] list already holds all N
#     members (a node answers 200 a beat before it finishes learning the others).
#   * Multicast loopback needs a multicast-capable interface up (en0 here). Offline with no
#     interface, discovery can't rendezvous -- that's an environment limit, not a bug.
#
# Usage:
#   harness/cluster_check.sh          # 2 nodes
#   harness/cluster_check.sh 3        # 3 nodes
#   FAILOVER=1 harness/cluster_check.sh 3   # and kill the leader: a new leader, writes resume, it rejoins
#   BIN=build/bin/xapiand harness/cluster_check.sh 2
set -u

N="${1:-2}"
HARNESS_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(dirname "$HARNESS_DIR")"
BIN="${BIN:-$REPO/build/bin/xapiand}"

HTTP_BASE=8880          # node i (1-based): http HTTP_BASE+i-1
REMOTE_BASE=9880        #                   xapian(remote)  REMOTE_BASE+i-1
REPLICA_BASE=7880       #                   replica(replication) REPLICA_BASE+i-1
DISCOVERY_PORT=58880    # SHARED across nodes (multicast rendezvous)
# Local interface for discovery multicast. Default to the loopback interface so a one-host
# cluster's gossip is self-contained and immune to a VPN/utun default route that can't
# carry multicast (INADDR_ANY/default-route multicast silently drops under most VPNs).
# Set DISCOVERY_IFACE= (empty) to use the default route (cross-host clusters).
DISCOVERY_IFACE="${DISCOVERY_IFACE:-127.0.0.1}"
RUNDIR="${RUNDIR:-$REPO/build/xcluster_$$}"
# The node pinned as holder of the primary cluster database; empty lets it follow the Raft
# leader, as failover needs. FAILOVER=1 leaves it empty unless set.
if [ "${FAILOVER:-0}" = "1" ]; then PRIMARY_NODE="${PRIMARY_NODE-}"; else PRIMARY_NODE="${PRIMARY_NODE-node1}"; fi
INDEX="cluster_probe"

if [ ! -x "$BIN" ]; then echo "missing binary: $BIN (build it or set BIN=...)"; exit 1; fi

PIDS=()
cleanup() {
	for p in "${PIDS[@]:-}"; do [ -n "$p" ] && kill "$p" 2>/dev/null; done
	# give them a moment, then verify none of OUR pids survive
	sleep 1
	for p in "${PIDS[@]:-}"; do [ -n "$p" ] && kill -0 "$p" 2>/dev/null && kill -9 "$p" 2>/dev/null; done
	[ "${KEEP_RUNDIR:-0}" = "1" ] || rm -rf "$RUNDIR"
}
trap cleanup EXIT

# Any leftover node squatting on our ports makes a fresh node fail to bind (EADDRINUSE)
# and exit, after which curl silently hits the STALE node -- a bogus result. Fail fast.
if command -v lsof >/dev/null 2>&1; then
	occupied=0
	for port in $(seq "$HTTP_BASE" $((HTTP_BASE + N - 1))) \
	            $(seq "$REMOTE_BASE" $((REMOTE_BASE + N - 1))) \
	            $(seq "$REPLICA_BASE" $((REPLICA_BASE + N - 1))) \
	            "$DISCOVERY_PORT"; do
		pids="$(lsof -tiTCP:"$port" -sTCP:LISTEN 2>/dev/null | tr '\n' ' ')"
		if [ -n "$pids" ]; then
			echo "port $port is already occupied by pid(s): $pids"
			occupied=1
		fi
	done
	[ "$occupied" = "0" ] || exit 1
fi
rm -rf "$RUNDIR"; mkdir -p "$RUNDIR"

http_port() { echo $((HTTP_BASE + $1 - 1)); }

launch_node() {
	local i="$1" name="node$1" dir="$RUNDIR/node$1"
	mkdir -p "$dir"
	"$BIN" --port "$(http_port "$i")" \
	       --xapian-port  $((REMOTE_BASE  + i - 1)) \
	       --replica-port $((REPLICA_BASE + i - 1)) \
	       --discovery-port "$DISCOVERY_PORT" \
	       ${DISCOVERY_IFACE:+--discovery-interface "$DISCOVERY_IFACE"} \
	       --name "$name" ${PRIMARY_NODE:+--primary-node "$PRIMARY_NODE"} -D "$dir" \
	       ${FAILOVER:+--database-stall-time 5} \
	       >"$RUNDIR/node$i.log" 2>&1 &
	PIDS[$i]=$!
	echo "launched $name (pid ${PIDS[$i]}) http=$(http_port "$i") remote=$((REMOTE_BASE+i-1)) replica=$((REPLICA_BASE+i-1))"
}

# names[] a node currently sees in its own GET / membership list
seen_names() {
	curl -s --max-time 4 "localhost:$1/" 2>/dev/null | \
		python3 -c "import sys,json
try: d=json.load(sys.stdin)
except Exception: sys.exit(0)
print(' '.join(n['name'] for n in d.get('nodes',[])))" 2>/dev/null
}

# wait until node on $1 both answers 200 AND sees all N members; $2 = deadline seconds
wait_member() {
	local port="$1" deadline=$(( $(date +%s) + $2 ))
	while [ "$(date +%s)" -lt "$deadline" ]; do
		# a launched node that already exited (bind clash) -> fail fast
		local names; names="$(seen_names "$port")"
		local count; count="$(echo "$names" | wc -w | tr -d ' ')"
		[ "$count" -ge "$N" ] && { echo "$names"; return 0; }
		sleep 1
	done
	echo "$(seen_names "$port")"
	return 1
}

FAIL=0
check() { if [ "$1" = "0" ]; then echo "  ok:   $2"; else echo "  FAIL: $2"; FAIL=$((FAIL+1)); fi; }

echo "== cluster_check: $N nodes =="

# --- bring the cluster up (primary first, then the rest) ---
launch_node 1
echo -n "node1 settling"; for _ in $(seq 1 60); do
	[ "$(curl -s -o /dev/null -w '%{http_code}' --max-time 2 "localhost:$HTTP_BASE/" 2>/dev/null)" = "200" ] && break
	kill -0 "${PIDS[1]}" 2>/dev/null || { echo " -- node1 died:"; grep -iE 'error|bind|exception' "$RUNDIR/node1.log" | head -3; exit 1; }
	echo -n .; sleep 1
done; echo
for i in $(seq 2 "$N"); do launch_node "$i"; done

# --- 1. DISCOVERY: every node converges on the full membership + one leader ---
echo "[1] discovery / membership"
allseen=1
for i in $(seq 1 "$N"); do
	port="$(http_port "$i")"
	names="$(wait_member "$port" 90)"
	got="$(echo "$names" | wc -w | tr -d ' ')"
	echo "     node$i sees: [$names] ($got/$N)"
	[ "$got" -ge "$N" ] || allseen=0
done
check "$([ "$allseen" = "1" ] && echo 0 || echo 1)" "all $N nodes see the full membership"
leaders="$(curl -s --max-time 5 "localhost:$HTTP_BASE/" | python3 -c "import sys,json;d=json.load(sys.stdin);print(sum(1 for n in d.get('nodes',[]) if n.get('leader')))" 2>/dev/null)"
check "$([ "$leaders" = "1" ] && echo 0 || echo 1)" "exactly one leader elected (got ${leaders:-none})"

# --- 2. REPLICATION / REMOTE: write on node1, read from every other node ---
# The first write to a fresh cluster returns 2xx once the primary shard is written, but
# the sibling shards on other nodes are still being created/replicated asynchronously
# (the "Checked out shard is taking too long" churn in the logs). So the cross-node read
# is eventually-consistent: poll it until the value shows up or a deadline passes.
echo "[2] write on node1, read cross-node (remote/replication)"
put="$(curl -s --max-time 60 -XPUT "localhost:$HTTP_BASE/$INDEX/1" \
	-H 'Content-Type: application/json' -d '{"title":"hello cluster","n":42}' \
	-w '%{http_code}' -o /dev/null)"
check "$([ "$put" = "200" ] || [ "$put" = "201" ] || [ "$put" = "204" ] && echo 0 || echo 1)" "PUT doc on node1 (http $put)"
for i in $(seq 2 "$N"); do
	port="$(http_port "$i")"
	r=1; last=""
	deadline=$(( $(date +%s) + 90 ))
	while [ "$(date +%s)" -lt "$deadline" ]; do
		last="$(curl -s --max-time 45 "localhost:$port/$INDEX/1")"
		echo "$last" | grep -q '"n":42' && { r=0; break; }
		sleep 2
	done
	[ "$r" = 0 ] || echo "     node$i last read: $(echo "$last" | head -c 160)"
	check "$r" "GET the doc from node$i returns the written value"
done

# --- 3. DISTRIBUTED SEARCH: query the last node, hits gather across all shards ---
echo "[3] distributed search from node$N"
lastport="$(http_port "$N")"
s=1; last=""
deadline=$(( $(date +%s) + 90 ))
while [ "$(date +%s)" -lt "$deadline" ]; do
	last="$(curl -s --max-time 45 -XSEARCH "localhost:$lastport/$INDEX/" \
		-H 'Content-Type: application/json' -d '{"_query":{"title":"hello"}}')"
	echo "$last" | python3 -c "import sys,json
try: d=json.load(sys.stdin)
except Exception: sys.exit(1)
sys.exit(0 if d.get('count',0)>=1 and any(h.get('n')==42 for h in d.get('hits',[])) else 1)" && { s=0; break; }
	sleep 2
done
[ "$s" = 0 ] || echo "     node$N last search: $(echo "$last" | head -c 160)"
check "$s" "SEARCH from node$N gathers the doc (count>=1, the hit present)"

# --- 3b. AGGREGATIONS: every node's sum / avg / count over all the shards equals the
# values written (compares the numbers, not the document counts). ---
echo "[3b] aggregations from every node"
AGG_INDEX="cluster_agg"; agg_n=40; agg_sum=0
for k in $(seq "$agg_n"); do
	a=$((k % 30 + 1)); agg_sum=$((agg_sum + a))
	curl -s --max-time 60 -XPUT "localhost:$HTTP_BASE/$AGG_INDEX/$k" -H 'Content-Type: application/json' \
		-d "{\"age\":$a}" -o /dev/null
done
# The second request asks for a tiny _check_at_least: aggregations still cover every match.
for cal in "" ',"_check_at_least":1'; do
for i in $(seq 1 "$N"); do
	port="$(http_port "$i")"
	r=1; last=""
	deadline=$(( $(date +%s) + 60 ))
	while [ "$(date +%s)" -lt "$deadline" ]; do
		last="$(curl -s --max-time 45 -XSEARCH "localhost:$port/$AGG_INDEX/?volatile" -H 'Content-Type: application/json' \
			-d "{\"_query\":\"*\",\"_limit\":0$cal,\"_aggs\":{\"s\":{\"_sum\":{\"_field\":\"age\"}},\"a\":{\"_avg\":{\"_field\":\"age\"}}}}")"
		echo "$last" | python3 -c "import sys,json
try: g=json.load(sys.stdin)['aggregations']
except Exception: sys.exit(1)
sys.exit(0 if g.get('_doc_count')==$agg_n and g['s']['_sum']==$agg_sum and abs(g['a']['_avg']-$agg_sum/$agg_n)<1e-9 else 1)" && { r=0; break; }
		sleep 2
	done
	[ "$r" = 0 ] || echo "     node$i last aggregation: $(echo "$last" | head -c 200)"
	check "$r" "node$i: _doc_count=$agg_n _sum=$agg_sum _avg=$(python3 -c "print($agg_sum/$agg_n)") over all shards${cal:+ (with$cal)}"
done
done

# --- 4. FAILOVER (FAILOVER=1, N >= 3): kill the leader. The survivors elect one new leader,
# and writes through a survivor resume and read back from another, before the killed node
# returns; then it rejoins under the one leader. Nodes start with a 5 s
# --database-stall-time, so the dead node's primary shards are promoted on their replicas
# within the check's window (the default waits 300 s); the primary cluster database follows
# the leader unless PRIMARY_NODE pins it. ---
if [ "${FAILOVER:-0}" = "1" ] && [ "$N" -ge 3 ]; then
	echo "[4] failover: kill the leader"
	leader="$(curl -s --max-time 5 "localhost:$HTTP_BASE/" | python3 -c "import sys,json;d=json.load(sys.stdin);print(next((n['name'] for n in d.get('nodes',[]) if n.get('leader')),''))" 2>/dev/null)"
	li="${leader#node}"
	if [ -z "$leader" ] || [ -z "${PIDS[$li]:-}" ]; then
		check 1 "a leader to kill (got '${leader:-none}')"
	else
		t0=$(date +%s)
		kill -9 "${PIDS[$li]}" 2>/dev/null; PIDS[$li]=""
		survivors=(); for i in $(seq 1 "$N"); do [ "$i" != "$li" ] && survivors+=("$i"); done
		a="$(http_port "${survivors[0]}")"; b="$(http_port "${survivors[1]}")"
		newleader=""
		deadline=$(( t0 + 60 ))
		while [ "$(date +%s)" -lt "$deadline" ]; do
			newleader="$(curl -s --max-time 5 "localhost:$a/" | python3 -c "import sys,json
d=json.load(sys.stdin); ls=[n['name'] for n in d.get('nodes',[]) if n.get('leader') and n.get('name')!='$leader']
print(ls[0] if len(ls)==1 else '')" 2>/dev/null)"
			[ -n "$newleader" ] && break
			sleep 1
		done
		check "$([ -n "$newleader" ] && echo 0 || echo 1)" "the survivors elect one new leader after $leader dies (got ${newleader:-none} in $(( $(date +%s) - t0 )) s)"
		put=""; deadline=$(( t0 + 120 ))
		while [ "$(date +%s)" -lt "$deadline" ]; do
			put="$(curl -s --max-time 30 -XPUT "localhost:$a/$INDEX/2" -H 'Content-Type: application/json' \
				-d '{"title":"after failover","n":43}' -w '%{http_code}' -o /dev/null)"
			{ [ "$put" = "200" ] || [ "$put" = "201" ] || [ "$put" = "204" ]; } && break
			sleep 1
		done
		check "$({ [ "$put" = "200" ] || [ "$put" = "201" ] || [ "$put" = "204" ]; } && echo 0 || echo 1)" "a write through node${survivors[0]} succeeds with $leader down (http $put, $(( $(date +%s) - t0 )) s after the kill)"
		r=1; deadline=$(( $(date +%s) + 90 ))
		while [ "$(date +%s)" -lt "$deadline" ]; do
			curl -s --max-time 45 "localhost:$b/$INDEX/2" | grep -q '"n":43' && { r=0; break; }
			sleep 2
		done
		check "$r" "it reads back from node${survivors[1]} with $leader down"
		launch_node "$li"
		wait_member "$(http_port "$li")" 90 >/dev/null && rj=0 || rj=1
		check "$rj" "$leader rejoins and sees all $N members"
		leaders="$(curl -s --max-time 5 "localhost:$a/" | python3 -c "import sys,json;d=json.load(sys.stdin);print(sum(1 for n in d.get('nodes',[]) if n.get('leader')))" 2>/dev/null)"
		check "$([ "$leaders" = "1" ] && echo 0 || echo 1)" "exactly one leader after $leader rejoins (got ${leaders:-none})"
	fi
fi

echo
if [ "$FAIL" -eq 0 ]; then
	echo "=== VERDICT: GREEN -- cluster discovery + replication/remote + distributed search all work ($N nodes) ==="
else
	echo "=== VERDICT: RED -- $FAIL check(s) failed; logs in $RUNDIR/node*.log ==="
	for i in $(seq 1 "$N"); do echo "--- node$i tail ---"; tail -4 "$RUNDIR/node$i.log"; done
fi
exit "$FAIL"
