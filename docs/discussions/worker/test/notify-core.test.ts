import assert from "node:assert/strict";
import test from "node:test";
import {
  commentPermalink,
  notificationMessage,
  notificationPayload,
  notificationRetryDelay,
  notificationShouldRetry,
  notifyKind,
  notifyKindFromUrl,
  type NotifyInput,
} from "../src/notify-core.ts";

const input: NotifyInput = {
  commentId: "c_123",
  author: "Alex",
  authorLogin: "alex",
  postTitle: "My Post",
  postTerm: "my-post",
  postUrl: "https://blog.example.com/blog/my-post/",
  siteName: "owner/blog",
  siteUrl: "https://blog.example.com",
  body: "A comment",
  isReply: false,
};

test("builds the same stable comment permalink used by the feed", () => {
  assert.equal(
    commentPermalink(input.postUrl, input.siteUrl, input.commentId),
    "https://blog.example.com/blog/my-post/#c_123",
  );
  assert.equal(commentPermalink(null, input.siteUrl, input.commentId), "https://blog.example.com#c_123");
});

test("uses the term and site URL fallbacks and identifies replies", () => {
  const text = notificationMessage({ ...input, postTitle: null, postUrl: null, isReply: true });
  assert.match(text, /^💬 \[owner\/blog\] New reply by Alex on “my-post”/);
  assert.match(text, /https:\/\/blog\.example\.com#c_123$/);
});

test("recognizes only configured providers", () => {
  assert.equal(notifyKind(" Discord "), "discord");
  assert.equal(notifyKind(""), null);
  assert.equal(notifyKind("discrod"), null);
});

test("infers standard providers from webhook URLs", () => {
  assert.equal(notifyKindFromUrl("https://discord.com/api/webhooks/123/token"), "discord");
  assert.equal(notifyKindFromUrl("https://hooks.slack.com/services/T/B/key"), "slack");
  assert.equal(notifyKindFromUrl("https://api.telegram.org/bot123:token/sendMessage"), "telegram");
  assert.equal(notifyKindFromUrl("https://example.com/webhook"), null);
  assert.equal(notifyKindFromUrl("not a URL"), null);
});

test("builds a mention-safe Discord payload", () => {
  assert.deepEqual(notificationPayload("discord", {}, "@everyone hello"), {
    content: "@everyone hello",
    allowed_mentions: { parse: [] },
  });
});

test("builds Slack and Telegram payloads", () => {
  assert.deepEqual(notificationPayload("slack", {}, "hello"), { text: "hello" });
  assert.deepEqual(notificationPayload("telegram", { telegramChat: "42" }, "hello"), {
    chat_id: "42",
    text: "hello",
    disable_web_page_preview: false,
  });
  assert.equal(notificationPayload("telegram", {}, "hello"), null);
});

test("retries only throttled and transient failures with a bounded delay", () => {
  assert.equal(notificationShouldRetry(429), true);
  assert.equal(notificationShouldRetry(503), true);
  assert.equal(notificationShouldRetry(400), false);
  assert.equal(notificationRetryDelay("2"), 2_000);
  assert.equal(notificationRetryDelay("60"), 5_000);
  assert.equal(notificationRetryDelay(null), 250);
});
