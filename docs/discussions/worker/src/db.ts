/**
 * D1Database driver for the Cloudflare D1 backing store.
 *
 * D1 uses SQLite semantics. Queries go through its prepared-statement API
 * (`.prepare(sql).bind(...).all()/.first()/.run()`, `.batch([...])` for atomic groups).
 * The schema ships through migrations, and sessions are stateless cookies.
 * Time columns are epoch seconds.
 */
import type { D1Database } from "@cloudflare/workers-types";

const now = (): number => Date.now() / 1000;

const COMMENT_COLS =
  "id, term, parent_id, author_login, author_subject, author_name, author_avatar, author_url, " +
  "body_md, body_html, created_at, updated_at, is_minimized, min_reason, hidden_at, tenant_id";

export interface CommentRow {
  id: string;
  term: string;
  parent_id: string | null;
  author_login: string;
  author_subject: string;
  author_name: string | null;
  author_avatar: string | null;
  author_url: string | null;
  body_md: string;
  body_html: string;
  created_at: number;
  updated_at: number | null;
  is_minimized: boolean;
  min_reason: string | null;
  hidden_at: number | null;
  tenant_id: string;
}

export interface TenantRow {
  id: string;
  active: boolean;
  config_ciphertext: string;
  config_nonce: string;
  created_at: number;
  updated_at: number | null;
}

export interface DiscussionRow {
  term: string;
  title: string | null;
  subtitle: string | null;
  url: string | null;
  createdAt: number;
}

/** A recent comment joined to its post (title/url), for the owner's Atom feed. */
export interface RecentComment {
  id: string;
  term: string;
  parent_id: string | null;
  author_login: string;
  author_name: string | null;
  body_md: string;
  body_html: string;
  created_at: number;
  post_title: string | null;
  post_url: string | null;
}

export interface ReactionGroup {
  content: string;
  count: number;
  viewerHasReacted: boolean;
}

function toCommentRow(r: Record<string, unknown>): CommentRow {
  return {
    id: r.id as string,
    term: r.term as string,
    parent_id: (r.parent_id as string | null) ?? null,
    author_login: r.author_login as string,
    author_subject: (r.author_subject as string) || "",
    author_name: (r.author_name as string | null) ?? null,
    author_avatar: (r.author_avatar as string | null) ?? null,
    author_url: (r.author_url as string | null) ?? null,
    body_md: r.body_md as string,
    body_html: r.body_html as string,
    created_at: r.created_at as number,
    updated_at: (r.updated_at as number | null) ?? null,
    is_minimized: Boolean(r.is_minimized),
    min_reason: (r.min_reason as string | null) ?? null,
    hidden_at: (r.hidden_at as number | null) ?? null,
    tenant_id: (r.tenant_id as string) ?? "",
  };
}

export class Database {
  constructor(private db: D1Database) {}

  // --- tenants ---------------------------------------------------------------
  async tenantGet(tenantId: string): Promise<TenantRow | null> {
    const r = await this.db
      .prepare("SELECT id, active, config_ciphertext, config_nonce, created_at, updated_at FROM tenants WHERE id=?")
      .bind(tenantId)
      .first<Record<string, unknown>>();
    if (!r) return null;
    return {
      id: r.id as string,
      active: Boolean(r.active),
      config_ciphertext: String(r.config_ciphertext || ""),
      config_nonce: String(r.config_nonce || ""),
      created_at: Number(r.created_at),
      updated_at: r.updated_at == null ? null : Number(r.updated_at),
    };
  }

  async tenantPut(tenantId: string, active: boolean, ciphertext: string, nonce: string): Promise<boolean> {
    const existed = Boolean(await this.tenantGet(tenantId));
    await this.db.prepare(
      "INSERT INTO tenants(id, origin, repo, repo_url, created_at, active, config_ciphertext, config_nonce, updated_at) " +
      "VALUES(?, '', '', '', ?, ?, ?, ?, ?) " +
      "ON CONFLICT(id) DO UPDATE SET active=excluded.active, config_ciphertext=excluded.config_ciphertext, " +
      "config_nonce=excluded.config_nonce, updated_at=excluded.updated_at",
    ).bind(tenantId, now(), active ? 1 : 0, ciphertext, nonce, now()).run();
    return !existed;
  }

  // --- discussions -----------------------------------------------------------
  async discussionGet(tenantId: string, term: string): Promise<DiscussionRow | null> {
    const r = await this.db
      .prepare("SELECT term, title, subtitle, url, created_at FROM discussions WHERE tenant_id=? AND term=?")
      .bind(tenantId, term)
      .first<Record<string, unknown>>();
    if (!r) return null;
    return {
      term: r.term as string,
      title: (r.title as string | null) ?? null,
      subtitle: (r.subtitle as string | null) ?? null,
      url: (r.url as string | null) ?? null,
      createdAt: r.created_at as number,
    };
  }

  async discussionUpsert(
    tenantId: string,
    term: string,
    title: string | null,
    subtitle: string | null,
    url: string | null,
  ): Promise<void> {
    await this.db
      .prepare(
        "INSERT OR IGNORE INTO discussions(tenant_id, term, title, subtitle, url, created_at) VALUES(?,?,?,?,?,?)",
      )
      .bind(tenantId, term, title, subtitle, url, now())
      .run();
  }

  // --- comments --------------------------------------------------------------
  async commentInsert(c: CommentRow): Promise<void> {
    await this.db
      .prepare(`INSERT INTO comments(${COMMENT_COLS}) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)`)
      .bind(
        c.id,
        c.term,
        c.parent_id,
        c.author_login,
        c.author_subject,
        c.author_name,
        c.author_avatar,
        c.author_url,
        c.body_md,
        c.body_html,
        c.created_at,
        c.updated_at,
        c.is_minimized ? 1 : 0,
        c.min_reason,
        c.hidden_at,
        c.tenant_id,
      )
      .run();
  }

  async commentGet(commentId: string): Promise<CommentRow | null> {
    const r = await this.db
      .prepare(`SELECT ${COMMENT_COLS} FROM comments WHERE id=?`)
      .bind(commentId)
      .first<Record<string, unknown>>();
    return r ? toCommentRow(r) : null;
  }

  async commentUpdateBody(commentId: string, bodyMd: string, bodyHtml: string, updatedAt: number): Promise<void> {
    await this.db
      .prepare("UPDATE comments SET body_md=?, body_html=?, updated_at=? WHERE id=?")
      .bind(bodyMd, bodyHtml, updatedAt, commentId)
      .run();
  }

  async commentSetHidden(
    commentId: string,
    hide: boolean,
    reason: string | null,
    hiddenAt: number | null,
  ): Promise<void> {
    await this.db
      .prepare("UPDATE comments SET is_minimized=?, min_reason=?, hidden_at=? WHERE id=?")
      .bind(hide ? 1 : 0, hide ? reason : null, hide ? hiddenAt : null, commentId)
      .run();
  }

  async commentDelete(commentId: string): Promise<string[]> {
    const kids = await this.db
      .prepare("SELECT id FROM comments WHERE parent_id=?")
      .bind(commentId)
      .all<Record<string, unknown>>();
    const ids = [commentId, ...(kids.results || []).map((r) => r.id as string)];
    const qs = ids.map(() => "?").join(",");
    await this.db.prepare(`DELETE FROM comments WHERE id IN (${qs})`).bind(...ids).run();
    return ids;
  }

  async commentsTop(tenantId: string, term: string, limit: number, offset: number): Promise<CommentRow[]> {
    const r = await this.db
      .prepare(
        `SELECT ${COMMENT_COLS} FROM comments ` +
          "WHERE tenant_id=? AND term=? AND parent_id IS NULL " +
          "ORDER BY created_at ASC, id ASC LIMIT ? OFFSET ?",
      )
      .bind(tenantId, term, limit, offset)
      .all<Record<string, unknown>>();
    return (r.results || []).map(toCommentRow);
  }

  async commentsTopCount(tenantId: string, term: string): Promise<number> {
    const r = await this.db
      .prepare("SELECT COUNT(*) AS n FROM comments WHERE tenant_id=? AND term=? AND parent_id IS NULL")
      .bind(tenantId, term)
      .first<Record<string, unknown>>();
    return (r?.n as number) ?? 0;
  }

  /** The most recent (non-hidden) comments across a whole tenant, newest first, each joined
   * to its post title/url. Powers the owner's private Atom feed. */
  async commentsRecent(tenantId: string, limit: number): Promise<RecentComment[]> {
    const r = await this.db
      .prepare(
        "SELECT c.id, c.term, c.parent_id, c.author_login, c.author_name, c.body_md, c.body_html, c.created_at, " +
          "d.title AS post_title, d.url AS post_url " +
          "FROM comments c LEFT JOIN discussions d ON d.tenant_id = c.tenant_id AND d.term = c.term " +
          "WHERE c.tenant_id=? AND c.hidden_at IS NULL " +
          "ORDER BY c.created_at DESC, c.id DESC LIMIT ?",
      )
      .bind(tenantId, limit)
      .all<Record<string, unknown>>();
    return (r.results || []).map((x) => ({
      id: String(x.id),
      term: String(x.term),
      parent_id: (x.parent_id as string) ?? null,
      author_login: String(x.author_login),
      author_name: (x.author_name as string) ?? null,
      body_md: String(x.body_md ?? ""),
      body_html: String(x.body_html ?? ""),
      created_at: Number(x.created_at),
      post_title: (x.post_title as string) ?? null,
      post_url: (x.post_url as string) ?? null,
    }));
  }

  async commentsReplies(parentIds: string[]): Promise<Record<string, CommentRow[]>> {
    const ids = parentIds.filter(Boolean);
    if (!ids.length) return {};
    const qs = ids.map(() => "?").join(",");
    const r = await this.db
      .prepare(
        `SELECT ${COMMENT_COLS} FROM comments WHERE parent_id IN (${qs}) ORDER BY created_at ASC, id ASC`,
      )
      .bind(...ids)
      .all<Record<string, unknown>>();
    const out: Record<string, CommentRow[]> = {};
    for (const raw of r.results || []) {
      const row = toCommentRow(raw);
      (out[row.parent_id as string] ??= []).push(row);
    }
    return out;
  }

  // --- reactions -------------------------------------------------------------
  async reactionsFor(commentIds: string[], viewer: string | null): Promise<Record<string, ReactionGroup[]>> {
    const ids = [...new Set(commentIds.filter(Boolean))];
    if (!ids.length) return {};
    const qs = ids.map(() => "?").join(",");
    const r = await this.db
      .prepare(
        "SELECT comment_id, content, COUNT(*) AS c, " +
          "SUM(CASE WHEN login=? THEN 1 ELSE 0 END) AS mine " +
          `FROM reactions WHERE comment_id IN (${qs}) GROUP BY comment_id, content`,
      )
      .bind(viewer || "", ...ids)
      .all<Record<string, unknown>>();
    const out: Record<string, ReactionGroup[]> = {};
    for (const row of r.results || []) {
      const cid = row.comment_id as string;
      (out[cid] ??= []).push({
        content: row.content as string,
        count: row.c as number,
        viewerHasReacted: Boolean(row.mine),
      });
    }
    return out;
  }

  async reactToggle(commentId: string, login: string, content: string, on: boolean, tenantId: string): Promise<void> {
    if (on) {
      await this.db
        .prepare(
          "INSERT OR IGNORE INTO reactions(comment_id, login, content, created_at, tenant_id) VALUES(?,?,?,?,?)",
        )
        .bind(commentId, login, content, now(), tenantId)
        .run();
    } else {
      await this.db
        .prepare("DELETE FROM reactions WHERE comment_id=? AND login=? AND content=?")
        .bind(commentId, login, content)
        .run();
    }
  }

  async reactionsPurge(commentId: string): Promise<void> {
    await this.db.prepare("DELETE FROM reactions WHERE comment_id=?").bind(commentId).run();
  }
}
