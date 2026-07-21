/**
 * A private Atom feed of the most recent comments across a tenant, so the blog owner can
 * subscribe in any RSS reader and see new comments without email or a webhook. The
 * tenant's configured feed token gates the URL.
 */
import type { RecentComment } from "./db.js";
import { commentPermalink } from "./notify-core.js";
import type { TenantConfig } from "./tenant-config.js";

function xmlEscape(s: string): string {
  return (s || "")
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&apos;");
}

function iso(epochSec: number): string {
  return new Date(epochSec * 1000).toISOString().replace(/\.\d{3}Z$/, "Z");
}

function excerpt(s: string, n = 500): string {
  const one = (s || "").replace(/\s+/g, " ").trim();
  return one.length > n ? one.slice(0, n - 1) + "\u2026" : one;
}

// A stable timestamp for the empty-feed placeholder, so the feed doesn't churn its
// <updated> on every poll before any real comment exists.
const EMPTY_TS = "2020-01-01T00:00:00Z";

function entry(parts: string[]): string {
  return ["  <entry>", ...parts, "  </entry>"].join("\n");
}

export function atomFeed(config: TenantConfig, feedId: string, rows: RecentComment[]): string {
  const site = config.site.url;
  const author = config.site.repo || site || "Comments";
  const updated = rows.length ? iso(rows[0].created_at) : EMPTY_TS;
  // A brand-new backend has no comments, and many readers refuse to subscribe to a feed
  // with zero entries, so emit one stable placeholder until a real comment arrives.
  const items = rows.length
    ? rows.map((r) => {
        const postName = r.post_title || r.term;
        const who = r.author_name || r.author_login;
        const kind = r.parent_id ? "reply" : "comment";
        // Deep-link straight to the comment on its post: the widget sets each comment's
        // DOM id and scroll-highlights `#<comment-id>` (the same URL its "Copy link" makes).
        const permalink = commentPermalink(r.post_url, site, r.id) as string;
        return entry([
          `    <title>${xmlEscape(`${who} \u2014 ${kind} on ${postName}`)}</title>`,
          `    <link href="${xmlEscape(permalink)}"/>`,
          `    <id>${xmlEscape(permalink)}</id>`,
          `    <updated>${iso(r.created_at)}</updated>`,
          `    <published>${iso(r.created_at)}</published>`,
          `    <author><name>${xmlEscape(who)}</name></author>`,
          `    <summary type="text">${xmlEscape(excerpt(r.body_md))}</summary>`,
          `    <content type="html">${xmlEscape(r.body_html || "")}</content>`,
        ]);
      })
    : [
        entry([
          "    <title>No comments yet</title>",
          `    <link href="${xmlEscape(site)}"/>`,
          `    <id>${xmlEscape(`${feedId}#placeholder`)}</id>`,
          `    <updated>${EMPTY_TS}</updated>`,
          `    <published>${EMPTY_TS}</published>`,
          `    <author><name>${xmlEscape(author)}</name></author>`,
          "    <summary type=\"text\">New comments will appear here as they are posted.</summary>",
        ]),
      ];
  return [
    '<?xml version="1.0" encoding="utf-8"?>',
    '<feed xmlns="http://www.w3.org/2005/Atom">',
    `  <title>${xmlEscape(`Comments \u2014 ${config.site.repo || site}`)}</title>`,
    `  <id>${xmlEscape(feedId)}</id>`,
    `  <updated>${updated}</updated>`,
    `  <icon>${xmlEscape(site.replace(/\/+$/, "") + "/favicon.ico")}</icon>`,
    `  <author><name>${xmlEscape(author)}</name></author>`,
    `  <link href="${xmlEscape(site)}"/>`,
    `  <link rel="self" href="${xmlEscape(feedId)}"/>`,
    items.join("\n"),
    "</feed>",
    "",
  ].join("\n");
}
