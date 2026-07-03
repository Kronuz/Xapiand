#!/usr/bin/env python3
#
# discovery_sniff.py [seconds] [interface]
#
# Joins the Xapiand discovery multicast group and decodes every datagram's header --
# [major][minor][type][serialise_string(cluster_name)] -- printing a per-message-type
# histogram + the first messages in sequence. It is a BEFORE/AFTER oracle for the Discovery
# -> cluster::Bus + cluster::Raft cutover: the wire frame is byte-compatible, so the message
# mix and handshake/Raft ordering must be IDENTICAL before and after. Run it alongside
# harness/cluster_check.sh and diff the output.
#
#   harness/discovery_sniff.py 90 127.0.0.1 > /tmp/before.txt   # current (libev) build
#   ... run cluster_check.sh in another shell ...
#   (after the cutover) harness/discovery_sniff.py 90 127.0.0.1 > /tmp/after.txt
#   diff <(grep HIST /tmp/before.txt) <(grep HIST /tmp/after.txt)

import collections
import socket
import struct
import sys
import time

GROUP = "239.192.168.1"
PORT = 58880

# DiscoveryMessage enum order (discovery.h ENUM_CLASS -> 0,1,2,...).
TYPES = [
    "CLUSTER_HELLO", "CLUSTER_WAVE", "CLUSTER_SNEER", "CLUSTER_ENTER", "CLUSTER_BYE",
    "RAFT_HEARTBEAT", "RAFT_HEARTBEAT_RESPONSE", "RAFT_APPEND_ENTRIES",
    "RAFT_APPEND_ENTRIES_RESPONSE", "RAFT_REQUEST_VOTE", "RAFT_REQUEST_VOTE_RESPONSE",
    "RAFT_ADD_COMMAND", "DB_UPDATED", "SCHEMA_UPDATED", "INDEX_SETTINGS_UPDATED",
    "PRIMARY_UPDATED", "ELECT_PRIMARY", "ELECT_PRIMARY_RESPONSE",
]


def unserialise_length(buf, i):
    # varint, byte-compatible with Xapiand's serialise_length.
    n = buf[i]; i += 1
    if n == 0xff:
        n = 0; shift = 0
        while True:
            ch = buf[i]; i += 1
            n |= (ch & 0x7f) << shift
            shift += 7
            if ch & 0x80:
                break
        n += 255
    return n, i


def main():
    seconds = float(sys.argv[1]) if len(sys.argv) > 1 else 90.0
    iface = sys.argv[2] if len(sys.argv) > 2 else "127.0.0.1"

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except OSError:
        pass
    s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(iface))
    mreq = socket.inet_aton(GROUP) + socket.inet_aton(iface)
    s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    s.bind(("", PORT))
    s.settimeout(1.0)

    hist = collections.Counter()
    seq = []
    total = 0
    deadline = time.time() + seconds
    print("sniffing discovery %s:%d on %s for %.0fs ..." % (GROUP, PORT, iface, seconds))
    while time.time() < deadline:
        try:
            data, _ = s.recvfrom(2048)
        except socket.timeout:
            continue
        if len(data) < 4:
            continue
        major, minor, mtype = data[0], data[1], data[2]
        name = TYPES[mtype] if 0 <= mtype < len(TYPES) else "UNKNOWN(%d)" % mtype
        try:
            tlen, i = unserialise_length(data, 3)
            token = data[i:i + tlen].decode("latin1")
        except Exception:
            token = "?"
        hist[name] += 1
        total += 1
        if len(seq) < 40:
            seq.append("%s(v%d.%d, token=%s, %dB)" % (name, major, minor, token, len(data)))

    print("SEQ (first %d):" % len(seq))
    for i, e in enumerate(seq):
        print("  %2d %s" % (i, e))
    print("HIST total=%d" % total)
    for name in TYPES:
        if hist[name]:
            print("HIST %-30s %d" % (name, hist[name]))
    for name, c in hist.items():
        if name not in TYPES:
            print("HIST %-30s %d" % (name, c))


if __name__ == "__main__":
    main()
