-- Initial schema for the self-hosted comments store on D1.
--
-- There is no `sessions` table: sessions are stateless HMAC-signed cookies
-- (see src/sessions.ts), so nothing session-related is persisted.
--
-- Time columns are epoch seconds stored as REAL. The Worker writes Date.now()/1000.

-- Tenants: one row per hosted blog. `origin` is the blog's site URL (the per-request
-- CORS/identity key); `repo`/`repo_url` are that blog's repo; strip_suffix/giphy_key
-- are that blog's widget config, served by GET /api/config.
CREATE TABLE IF NOT EXISTS tenants (
  id           TEXT PRIMARY KEY,
  origin       TEXT,
  repo         TEXT,
  repo_url     TEXT,
  created_at   REAL NOT NULL,
  strip_suffix TEXT NOT NULL DEFAULT '',
  giphy_key    TEXT NOT NULL DEFAULT ''
);

-- Per-tenant moderators (the blog owner + delegates). Seeded from ADMIN_LOGINS.
CREATE TABLE IF NOT EXISTS tenant_admins (
  tenant_id TEXT NOT NULL,
  login     TEXT NOT NULL,
  PRIMARY KEY (tenant_id, login)
);

-- Discussions: the per-page container, keyed by (tenant_id, term).
CREATE TABLE IF NOT EXISTS discussions (
  tenant_id  TEXT NOT NULL,
  term       TEXT NOT NULL,
  title      TEXT,
  subtitle   TEXT,
  url        TEXT,
  created_at REAL NOT NULL,
  PRIMARY KEY (tenant_id, term)
);

-- Comments (the items). Replies point at a top-level comment via parent_id (NULL =
-- top-level). A comment belongs to its discussion by (tenant_id, term). body_html is
-- the rendered Markdown, cached so we don't re-render on every read.
CREATE TABLE IF NOT EXISTS comments (
  id            TEXT PRIMARY KEY,
  term          TEXT NOT NULL,
  parent_id     TEXT,
  author_login  TEXT NOT NULL,
  author_name   TEXT,
  author_avatar TEXT,
  author_url    TEXT,
  body_md       TEXT NOT NULL,
  body_html     TEXT NOT NULL,
  created_at    REAL NOT NULL,
  updated_at    REAL,
  is_minimized  INTEGER NOT NULL DEFAULT 0,
  min_reason    TEXT,
  hidden_at     REAL,
  tenant_id     TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_comments_term ON comments(term, parent_id, created_at);
CREATE INDEX IF NOT EXISTS idx_comments_parent ON comments(parent_id);
CREATE INDEX IF NOT EXISTS idx_comments_tenant_term ON comments(tenant_id, term, parent_id, created_at);

-- Local reaction store: one row per (comment, user, emoji), keyed by the verified login.
CREATE TABLE IF NOT EXISTS reactions (
  comment_id TEXT NOT NULL,
  login      TEXT NOT NULL,
  content    TEXT NOT NULL,
  created_at REAL NOT NULL,
  tenant_id  TEXT NOT NULL DEFAULT '',
  PRIMARY KEY (comment_id, login, content)
);
CREATE INDEX IF NOT EXISTS idx_reactions_comment ON reactions(comment_id);
