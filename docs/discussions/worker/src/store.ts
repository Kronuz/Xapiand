/**
 * SelfHostedStore is the comment store whose system of record is D1.
 *
 * The full commenting system (comments, replies, edits, hides, reactions) lives in D1; OAuth is
 * used only to learn who the reader is; Markdown is rendered locally (md.render). Threads
 * are keyed by `term` (the post slug) within a tenant. Authorization is ours: a comment's
 * author may edit/delete it; a moderator of the comment's tenant may delete/hide any.
 */
import { HttpError } from "./config.js";
import type { CommentRow, Database, ReactionGroup } from "./db.js";
import * as md from "./md.js";
import type { TenantContext } from "./tenants.js";

export interface Viewer {
  subject: string;
  login: string;
  name: string;
  avatarUrl: string;
  url?: string | null;
  tenant_id: string;
  is_admin: boolean;
}

// GitHub's eight reaction emoji (the ReactionContent enum); we whitelist these.
const REACTION_CONTENT = new Set([
  "THUMBS_UP",
  "THUMBS_DOWN",
  "LAUGH",
  "HOORAY",
  "CONFUSED",
  "HEART",
  "ROCKET",
  "EYES",
]);

// GitHub's ReportedContentClassifiers define the valid reasons for hiding a comment.
const HIDE_REASONS = new Set(["OUTDATED", "OFF_TOPIC", "RESOLVED", "DUPLICATE", "SPAM", "ABUSE"]);

/** Epoch seconds -> ISO-8601 UTC (what the widget's `new Date(...)` expects), seconds
 * precision so timestamps remain stable across reads. */
function iso(epoch: number | null | undefined): string | null {
  if (epoch === null || epoch === undefined) return null;
  return new Date(epoch * 1000).toISOString().replace(/\.\d{3}Z$/, "Z");
}

function requireViewer(viewer: Viewer | null): Viewer {
  if (!viewer) throw new HttpError(401, "sign in required");
  return viewer;
}

function checkReaction(content: string): string {
  const c = (content || "").toUpperCase();
  if (!REACTION_CONTENT.has(c)) throw new HttpError(400, "invalid reaction");
  return c;
}

interface ApiComment {
  id: string;
  url: string;
  createdAt: string | null;
  updatedAt: string | null;
  bodyHTML: string;
  bodyMarkdown: string;
  authorLogin: string;
  author: { login: string; name: string; url: string; avatarUrl: string };
  isMinimized: boolean;
  minimizedReason: string | null;
  hiddenAt: string | null;
  reactions: ReactionGroup[];
  replies?: ApiComment[];
  replyCount?: number;
  repliesHaveMore?: boolean;
}

function toDict(
  row: CommentRow,
  reactions: Record<string, ReactionGroup[]>,
  replies?: ApiComment[] | null,
  replyTotal?: number,
): ApiComment {
  const d: ApiComment = {
    id: row.id,
    // No GitHub URL in this store; link to the comment's anchor on the page.
    url: "#" + row.id,
    createdAt: iso(row.created_at),
    updatedAt: iso(row.updated_at),
    bodyHTML: row.body_html,
    bodyMarkdown: row.body_md,
    authorLogin: row.author_login,
    author: {
      login: row.author_login,
      name: row.author_name || row.author_login,
      url: row.author_url || "",
      avatarUrl: row.author_avatar || "",
    },
    isMinimized: row.is_minimized,
    minimizedReason: row.min_reason,
    hiddenAt: iso(row.hidden_at),
    reactions: reactions[row.id] || [],
  };
  if (replies !== undefined && replies !== null) {
    d.replies = replies;
    d.replyCount = replyTotal !== undefined ? replyTotal : replies.length;
    d.repliesHaveMore = false; // we return every reply, so never "more on GitHub"
  }
  return d;
}

export class SelfHostedStore {
  constructor(
    private db: Database,
    private tenants: TenantContext,
    private maxBody: number,
  ) {}

  /** A moderator of the comment's OWN tenant may hide/delete it (per-tenant). */
  private canModerate(row: CommentRow, viewer: Viewer): boolean {
    return this.tenants.isAdmin(row.tenant_id, viewer.subject, viewer.login);
  }

  async getDiscussion(opts: {
    tenantId: string;
    term: string | null;
    after: string | null;
    first: number;
    viewer: Viewer | null;
  }): Promise<unknown> {
    const { tenantId, term, after, first, viewer } = opts;
    if (!term) throw new HttpError(400, "term required");
    const viewerLogin = viewer ? viewer.login : null;
    let offset = 0;
    if (after) {
      const n = parseInt(after, 10);
      offset = Number.isFinite(n) ? Math.max(0, n) : 0;
    }
    let top = await this.db.commentsTop(tenantId, term, first + 1, offset);
    const hasNext = top.length > first;
    top = top.slice(0, first);
    const replyMap = await this.db.commentsReplies(top.map((t) => t.id));

    const ids: string[] = [];
    for (const t of top) {
      ids.push(t.id);
      for (const r of replyMap[t.id] || []) ids.push(r.id);
    }
    const rmap = await this.db.reactionsFor(ids, viewerLogin);

    const comments: ApiComment[] = [];
    for (const t of top) {
      const reps = (replyMap[t.id] || []).map((r) => toDict(r, rmap));
      comments.push(toDict(t, rmap, reps, reps.length));
    }

    const total = await this.db.commentsTopCount(tenantId, term);
    const disc = await this.db.discussionGet(tenantId, term);
    return {
      discussion: {
        totalCount: total,
        title: disc ? disc.title : null,
        url: disc ? disc.url : null,
      },
      pageInfo: { hasNextPage: hasNext, endCursor: hasNext ? String(offset + first) : null },
      comments,
    };
  }

  async addComment(opts: {
    tenantId: string;
    term: string | null;
    title: string | null;
    subtitle: string | null;
    url: string | null;
    body: string;
    replyToId: string | null;
    viewer: Viewer | null;
  }): Promise<ApiComment> {
    const viewer = requireViewer(opts.viewer);
    const { tenantId, term, title, subtitle, url, body } = opts;
    let replyToId = opts.replyToId;
    if (!term) throw new HttpError(400, "term required");
    if (!(body || "").trim()) throw new HttpError(400, "empty comment");
    if (body.length > this.maxBody) throw new HttpError(413, "comment too long");
    if (replyToId) {
      const parent = await this.db.commentGet(replyToId);
      if (!parent || parent.term !== term || parent.tenant_id !== tenantId) {
        throw new HttpError(404, "parent comment not found");
      }
      if (parent.parent_id) {
        // A thread is one level deep: a reply to a reply attaches to the top-level
        // comment, matching GitHub.
        replyToId = parent.parent_id;
      }
    }
    await this.db.discussionUpsert(tenantId, term, title, subtitle, url);
    const html = await md.render(body);
    const nowSec = Date.now() / 1000;
    const row: CommentRow = {
      id: "c_" + randomHex(8),
      term,
      parent_id: replyToId,
      author_login: viewer.login,
      author_subject: viewer.subject,
      author_name: viewer.name ?? null,
      author_avatar: viewer.avatarUrl ?? null,
      author_url: viewer.url ?? null,
      body_md: body,
      body_html: html,
      created_at: nowSec,
      updated_at: null,
      is_minimized: false,
      min_reason: null,
      hidden_at: null,
      tenant_id: tenantId,
    };
    await this.db.commentInsert(row);
    return toDict(row, {}, replyToId ? null : []);
  }

  async editComment(opts: { commentId: string; body: string; viewer: Viewer | null }): Promise<ApiComment> {
    const viewer = requireViewer(opts.viewer);
    const { commentId, body } = opts;
    if (!(body || "").trim()) throw new HttpError(400, "empty comment");
    if (body.length > this.maxBody) throw new HttpError(413, "comment too long");
    const row = await this.db.commentGet(commentId);
    if (!row || row.tenant_id !== viewer.tenant_id) throw new HttpError(404, "comment not found");
    if (!((row.author_subject ? row.author_subject === viewer.subject : row.author_login === viewer.login) || this.canModerate(row, viewer))) {
      throw new HttpError(403, "not allowed to edit this comment");
    }
    const html = await md.render(body);
    const nowSec = Date.now() / 1000;
    await this.db.commentUpdateBody(commentId, body, html, nowSec);
    row.body_md = body;
    row.body_html = html;
    row.updated_at = nowSec;
    return toDict(row, {});
  }

  async deleteComment(opts: { commentId: string; viewer: Viewer | null }): Promise<unknown> {
    const viewer = requireViewer(opts.viewer);
    const row = await this.db.commentGet(opts.commentId);
    if (!row || row.tenant_id !== viewer.tenant_id) throw new HttpError(404, "comment not found");
    if (!((row.author_subject ? row.author_subject === viewer.subject : row.author_login === viewer.login) || this.canModerate(row, viewer))) {
      throw new HttpError(403, "not allowed to delete this comment");
    }
    const deletedIds = await this.db.commentDelete(opts.commentId);
    for (const cid of deletedIds) await this.db.reactionsPurge(cid);
    return { ok: true, id: opts.commentId };
  }

  async setHidden(opts: {
    commentId: string;
    hide: boolean;
    reason: string | null;
    viewer: Viewer | null;
  }): Promise<ApiComment> {
    const viewer = requireViewer(opts.viewer);
    const row = await this.db.commentGet(opts.commentId);
    if (!row || row.tenant_id !== viewer.tenant_id) throw new HttpError(404, "comment not found");
    if (!this.canModerate(row, viewer)) throw new HttpError(403, "moderators only");
    let norm: string | null = null;
    if (opts.hide) {
      norm = (opts.reason || "OUTDATED").toUpperCase();
      if (!HIDE_REASONS.has(norm)) throw new HttpError(400, "invalid hide reason");
    }
    const hiddenAt = opts.hide ? Date.now() / 1000 : null;
    await this.db.commentSetHidden(opts.commentId, opts.hide, norm, hiddenAt);
    row.is_minimized = opts.hide;
    row.min_reason = norm;
    row.hidden_at = hiddenAt;
    return toDict(row, {});
  }

  async react(opts: {
    commentId: string;
    content: string;
    on: boolean;
    viewer: Viewer | null;
  }): Promise<unknown> {
    const viewer = requireViewer(opts.viewer);
    const content = checkReaction(opts.content);
    const row = await this.db.commentGet(opts.commentId);
    if (!row || row.tenant_id !== viewer.tenant_id) throw new HttpError(404, "comment not found");
    if (row.is_minimized) throw new HttpError(403, "cannot react to a hidden comment");
    await this.db.reactToggle(opts.commentId, viewer.login, content, opts.on, row.tenant_id);
    const groups = (await this.db.reactionsFor([opts.commentId], viewer.login))[opts.commentId] || [];
    return { comment_id: opts.commentId, reactions: groups };
  }

  async preview(opts: { text: string; viewer: Viewer | null }): Promise<string> {
    requireViewer(opts.viewer);
    const text = opts.text || "";
    if (text.length > this.maxBody) throw new HttpError(413, "comment too long");
    if (!text.trim()) return "";
    return md.render(text);
  }
}

/** A short random hex id (mirrors Python's secrets.token_hex). */
function randomHex(bytes: number): string {
  const buf = new Uint8Array(bytes);
  crypto.getRandomValues(buf);
  return [...buf].map((b) => b.toString(16).padStart(2, "0")).join("");
}
