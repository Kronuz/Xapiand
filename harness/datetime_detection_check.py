#!/usr/bin/env python3
"""Schema detection types a new field by its first value: strings datetime no longer parses are text.

Usage: datetime_detection_check.py PORT   (a running node on a fresh data directory)
"""
import http.client, json, sys

port = int(sys.argv[1])
failures = 0


def call(method, path, body=None):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=60)
    conn.request(method, path, body=body, headers={"Content-Type": "application/json"})
    resp = conn.getresponse()
    data = resp.read()
    conn.close()
    return resp.status, data


counter = 0


def detected(value):
    global counter
    counter += 1
    index = "/dt%d/" % counter
    status, data = call("PUT", index + "1?commit", json.dumps({"f": value}))
    if status >= 300:
        return "refused %d" % status
    status, data = call("GET", index + "._schema")
    return json.loads(data).get("f", {}).get("_type") if status == 200 else "schema %d" % status


def check(value, want):
    global failures
    got = detected(value)
    ok = got == want
    failures += not ok
    print("%-4s %-72s -> %s (want %s)" % ("ok" if ok else "FAIL", value[:72], got, want))


check("2020-01-01", "date")
check("2020-01-01T10:00:00", "datetime")
check("2020-01-01T10:00:00.5Z", "datetime")
check("2020-01-01T10:00:00." + "1" * 62, "datetime")
check("2020-01-01T10:00:00." + "1" * 63, "text")  # a fraction of more than 62 digits is not a datetime
check("2020-01-01T10:00:00.Z", "text")  # nor is a fraction dot with no digits
print("failures: %d" % failures)
sys.exit(1 if failures else 0)
