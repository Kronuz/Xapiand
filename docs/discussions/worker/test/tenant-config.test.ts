import assert from "node:assert/strict";
import test from "node:test";

import { accessKeyMatches, isAccessKey } from "../src/access.ts";

const key = "a".repeat(43);

test("an empty access key makes the tenant public", () => {
  assert.equal(isAccessKey(""), true);
  assert.equal(accessKeyMatches("", ""), true);
  assert.equal(accessKeyMatches("", "anything"), true);
});

test("a protected tenant accepts only its exact 32-byte base64url key", () => {
  assert.equal(isAccessKey(key), true);
  assert.equal(isAccessKey("not-a-32-byte-key"), false);
  assert.equal(accessKeyMatches(key, key), true);
  assert.equal(accessKeyMatches(key, "b".repeat(43)), false);
  assert.equal(accessKeyMatches(key, "short"), false);
});
