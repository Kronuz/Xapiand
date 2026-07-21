import { type Context, Hono } from "hono";
import { cors } from "hono/cors";
import { deleteCookie, getCookie, setCookie } from "hono/cookie";
import { type Cfg, type Env, HttpError, loadCfg } from "./config.js";
import { Database } from "./db.js";
import { atomFeed } from "./feed.js";
import * as oauthClient from "./gh.js";
import { notifyNewComment } from "./notify.js";
import { RateLimit } from "./ratelimit.js";
import { CookieSessionStore } from "./sessions.js";
import { SelfHostedStore, type Viewer } from "./store.js";
import { accessKeyMatches } from "./access.js";
import { encryptTenantConfig, publicTenantConfig, validateTenantConfig, validateTenantId, type TenantConfig } from "./tenant-config.js";
import { loadTenant, type TenantContext } from "./tenants.js";

type Vars = { cfg: Cfg; db: Database; tenantId: string; tenant: TenantContext; sessions: CookieSessionStore; store: SelfHostedStore };
type AppEnv = { Bindings: Env; Variables: Vars };
type Ctx = Context<AppEnv>;
const app = new Hono<AppEnv>();
const enc = new TextEncoder();
const COMMENT_TEST_TERM = "__comments-e2e__";
const rl = {
  read: new RateLimit(120, 60), post: new RateLimit(6, 60), edit: new RateLimit(30, 60),
  del: new RateLimit(30, 60), hide: new RateLimit(60, 60), preview: new RateLimit(30, 60), react: new RateLimit(60, 60),
};

function safeEq(a: string, b: string): boolean {
  if (a.length !== b.length) return false;
  let value = 0;
  for (let i = 0; i < a.length; i++) value |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return value === 0;
}

function clientKey(c: Ctx, viewer: Viewer | null): string {
  return viewer?.login ? `u:${c.get("tenantId")}:${viewer.login}` : `ip:${c.req.header("cf-connecting-ip") || "unknown"}`;
}

function limit(c: Ctx, limiter: RateLimit, viewer: Viewer | null): void {
  const retry = limiter.check(clientKey(c, viewer));
  if (retry !== null) throw new HttpError(429, "Too many requests; slow down.", { "Retry-After": String(retry) });
}

function tenantBase(c: Ctx): string {
  return `${c.get("cfg").publicBaseUrl}/${c.get("tenantId")}`;
}

function allowedReturn(config: TenantConfig, value: string): boolean {
  try { return config.origins.includes(new URL(value).origin); } catch { return false; }
}

async function currentSession(c: Ctx) {
  const auth = c.req.header("authorization");
  let value = auth?.startsWith("Bearer ") ? auth.slice(7).trim() : "";
  if (!value) value = getCookie(c, "gc_session") || "";
  if (!value) return null;
  const session = await c.get("sessions").get(value);
  return session?.tenantId === c.get("tenantId") ? session : null;
}

async function currentViewer(c: Ctx): Promise<Viewer | null> {
  const session = await currentSession(c);
  if (!session) return null;
  return {
    subject: session.subject, login: session.login, name: session.name, avatarUrl: session.avatarUrl, url: session.url,
    tenant_id: c.get("tenantId"), is_admin: c.get("tenant").isAdmin(c.get("tenantId"), session.subject, session.login),
  };
}

async function hmac(secret: string, value: string): Promise<string> {
  const key = await crypto.subtle.importKey("raw", enc.encode(secret), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  const bytes = new Uint8Array(await crypto.subtle.sign("HMAC", key, enc.encode(value)));
  return [...bytes].map((byte) => byte.toString(16).padStart(2, "0")).join("");
}

async function makeState(secret: string, tenantId: string, returnUrl: string): Promise<string> {
  const payload = btoa(JSON.stringify({ tenantId, returnUrl, issuedAt: Math.floor(Date.now() / 1000), nonce: crypto.randomUUID() }))
    .replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
  return `${payload}.${await hmac(secret, payload)}`;
}

async function readState(secret: string, signed: string): Promise<{ tenantId: string; returnUrl: string } | null> {
  const dot = signed.lastIndexOf(".");
  if (dot < 0) return null;
  const payload = signed.slice(0, dot);
  if (!safeEq(signed.slice(dot + 1), await hmac(secret, payload))) return null;
  try {
    let raw = payload.replace(/-/g, "+").replace(/_/g, "/");
    while (raw.length % 4) raw += "=";
    const value = JSON.parse(atob(raw)) as { tenantId: string; returnUrl: string; issuedAt: number };
    if (Math.abs(Date.now() / 1000 - value.issuedAt) > 600) return null;
    return value;
  } catch { return null; }
}

app.use("*", async (c, next) => {
  const length = c.req.header("content-length");
  const cfg = loadCfg(c.env);
  if (length && /^\d+$/.test(length) && Number(length) > cfg.requestMaxBytes) return c.json({ detail: "request too large" }, 413);
  c.set("cfg", cfg);
  c.set("db", new Database(c.env.DB));
  await next();
});

app.get("/health", (c) => c.json({ ok: true }));

// PUT is the complete management API: authenticated upsert of one full configuration.
app.put("/:tenant/config", async (c) => {
  const cfg = c.get("cfg");
  const token = c.req.header("authorization")?.replace(/^Bearer\s+/i, "") || "";
  if (!cfg.serviceAdminToken || !safeEq(token, cfg.serviceAdminToken)) throw new HttpError(401, "service administrator token required");
  const tenantId = c.req.param("tenant");
  validateTenantId(tenantId);
  const config = validateTenantConfig(await c.req.json(), c.env);
  const encrypted = await encryptTenantConfig(config, cfg.configMasterKey);
  const created = await c.get("db").tenantPut(tenantId, config.active, encrypted.ciphertext, encrypted.nonce);
  return c.json({
    tenant: tenantId,
    backendUrl: `${cfg.publicBaseUrl}/${tenantId}`,
    oauthCallbackUrl: `${cfg.publicBaseUrl}/${tenantId}/auth/callback`,
    config: publicTenantConfig(config),
  }, created ? 201 : 200);
});

// Every remaining tenant route loads the encrypted configuration selected by the path.
app.use("/:tenant/*", async (c, next) => {
  const tenantId = c.req.param("tenant");
  validateTenantId(tenantId);
  const tenant = await loadTenant(c.get("db"), c.get("cfg"), tenantId);
  c.set("tenantId", tenantId);
  c.set("tenant", tenant);
  c.set("sessions", new CookieSessionStore(c.get("cfg").sessionSecret, tenant.config.limits.sessionTtl));
  c.set("store", new SelfHostedStore(c.get("db"), tenant, tenant.config.limits.maxBody));
  await next();
});

app.use("/:tenant/*", (c, next) => cors({
  origin: (origin) => origin && c.get("tenant").config.origins.includes(origin) ? origin : null,
  credentials: true,
  allowMethods: ["GET", "POST", "PUT", "OPTIONS"],
  allowHeaders: ["Content-Type", "Authorization", "X-Discussions-Key"],
})(c, next));

// A non-empty tenant access key protects every browser-facing operation. The OAuth
// callback is authorized by its short-lived signed state, and the feed has its own token.
app.use("/:tenant/*", async (c, next) => {
  const expected = c.get("tenant").config.accessKey;
  if (!expected) return next();
  c.header("Cache-Control", "private, no-store");
  if (c.req.method === "OPTIONS") return next();
  const path = new URL(c.req.url).pathname;
  if (path.endsWith("/auth/callback") || path.endsWith("/comments/feed")) return next();
  if (!accessKeyMatches(expected, c.req.header("x-discussions-key") || "")) {
    return c.json({ detail: "not found" }, 404);
  }
  await next();
});

app.use("/:tenant/*", async (c, next) => {
  if (c.req.method !== "OPTIONS") {
    const origin = c.req.header("origin");
    const path = new URL(c.req.url).pathname;
    const exempt = path.endsWith("/comments/feed") || path.endsWith("/auth/callback");
    if (origin && !exempt && !c.get("tenant").config.origins.includes(origin)) return c.json({ detail: "origin not registered" }, 403);
  }
  await next();
});

app.get("/:tenant/config", (c) => c.json(publicTenantConfig(c.get("tenant").config)));

app.get("/:tenant/api/me", async (c) => {
  const session = await currentSession(c);
  const oauth = c.get("tenant").config.oauth;
  const base = { oauth: Boolean(oauth.clientId && oauth.clientSecret), oauthName: oauth.name, backend: "d1" };
  if (!session) return c.json({ ...base, authenticated: false });
  return c.json({ ...base, authenticated: true, login: session.login, avatarUrl: session.avatarUrl, name: session.name, isAdmin: c.get("tenant").isAdmin(c.get("tenantId"), session.subject, session.login) });
});

async function oauthAuthorizationUrl(c: Ctx): Promise<string> {
  const config = c.get("tenant").config;
  const returnUrl = c.req.query("return") || config.site.url;
  if (!allowedReturn(config, returnUrl)) throw new HttpError(400, "return URL is not allowed for this tenant");
  const redirectUri = `${tenantBase(c)}/auth/callback`;
  const params = new URLSearchParams({ client_id: config.oauth.clientId, redirect_uri: redirectUri, scope: config.oauth.scope, state: await makeState(c.get("cfg").sessionSecret, c.get("tenantId"), returnUrl) });
  return `${config.oauth.authorizeUrl}?${params}`;
}

app.get("/:tenant/auth/login", async (c) => {
  return c.redirect(await oauthAuthorizationUrl(c));
});

app.post("/:tenant/auth/login", async (c) => {
  return c.json({ url: await oauthAuthorizationUrl(c) });
});

app.get("/:tenant/auth/callback", async (c) => {
  const state = await readState(c.get("cfg").sessionSecret, c.req.query("state") || "");
  if (!state || state.tenantId !== c.get("tenantId") || !allowedReturn(c.get("tenant").config, state.returnUrl)) throw new HttpError(400, "bad state");
  const redirectUri = `${tenantBase(c)}/auth/callback`;
  const token = await oauthClient.exchangeCode(c.get("tenant").config.oauth, c.req.query("code") || "", redirectUri);
  const user = await oauthClient.user(c.get("tenant").config.oauth, token);
  const value = await c.get("sessions").create({ tenantId: c.get("tenantId"), subject: user.subject, login: user.login, name: user.name, avatarUrl: user.avatarUrl, url: user.profileUrl });
  setCookie(c, "gc_session", value, { httpOnly: true, sameSite: "None", secure: true, maxAge: c.get("tenant").config.limits.sessionTtl, path: `/${c.get("tenantId")}/` });
  return c.redirect(`${state.returnUrl.split("#")[0]}#gc_token=${encodeURIComponent(value)}`);
});

app.post("/:tenant/auth/logout", (c) => {
  deleteCookie(c, "gc_session", { path: `/${c.get("tenantId")}/`, sameSite: "None", secure: true });
  return c.json({ ok: true });
});

app.get("/:tenant/api/discussions", async (c) => {
  const viewer = await currentViewer(c); limit(c, rl.read, viewer);
  const first = Math.max(1, Math.min(Number(c.req.query("first") || 20), 100));
  return c.json(await c.get("store").getDiscussion({ tenantId: c.get("tenantId"), term: c.req.query("term") ?? null, after: c.req.query("after") ?? null, first, viewer }) as object);
});

app.get("/:tenant/comments/feed", async (c) => {
  const feed = c.get("tenant").config.feed;
  if (!feed.enabled || !feed.token || !safeEq(feed.token, c.req.query("token") || "")) throw new HttpError(404, "not found");
  limit(c, rl.read, null);
  const rows = await c.get("db").commentsRecent(c.get("tenantId"), 50);
  return c.body(atomFeed(c.get("tenant").config, `${tenantBase(c)}/comments/feed`, rows), 200, { "content-type": "application/atom+xml; charset=utf-8", "cache-control": "no-store" });
});

app.post("/:tenant/api/comments", async (c) => {
  const body = await c.req.json<{ body: string; term?: string; title?: string; subtitle?: string; url?: string; reply_to_id?: string }>();
  const viewer = await currentViewer(c);
  if (body.term === COMMENT_TEST_TERM && !viewer?.is_admin) throw new HttpError(403, "administrators only on the comment test thread");
  limit(c, rl.post, viewer);
  const created = await c.get("store").addComment({ tenantId: c.get("tenantId"), term: body.term ?? null, title: body.title ?? null, subtitle: body.subtitle ?? null, url: body.url ?? null, body: body.body, replyToId: body.reply_to_id ?? null, viewer });
  let execution: { waitUntil(p: Promise<unknown>): void } | undefined;
  try { execution = c.executionCtx; } catch { execution = undefined; }
  const config = c.get("tenant").config;
  notifyNewComment(config.notifications, execution, { commentId: created.id, author: viewer?.name || viewer?.login || "someone", authorLogin: viewer?.login || "", postTitle: body.title ?? null, postTerm: body.term ?? null, postUrl: body.url ?? null, siteName: config.site.repo || c.get("tenantId"), siteUrl: config.site.url, body: body.body, isReply: Boolean(body.reply_to_id) });
  return c.json(created as object);
});

app.post("/:tenant/api/comments/edit", async (c) => { const body = await c.req.json<{ comment_id: string; body: string }>(); const viewer = await currentViewer(c); limit(c, rl.edit, viewer); return c.json(await c.get("store").editComment({ commentId: body.comment_id, body: body.body, viewer }) as object); });
app.post("/:tenant/api/comments/delete", async (c) => { const body = await c.req.json<{ comment_id: string }>(); const viewer = await currentViewer(c); limit(c, rl.del, viewer); return c.json(await c.get("store").deleteComment({ commentId: body.comment_id, viewer }) as object); });
app.post("/:tenant/api/comments/hide", async (c) => { const body = await c.req.json<{ comment_id: string; hide?: boolean; reason?: string }>(); const viewer = await currentViewer(c); limit(c, rl.hide, viewer); return c.json(await c.get("store").setHidden({ commentId: body.comment_id, hide: body.hide ?? true, reason: body.reason ?? null, viewer }) as object); });
app.post("/:tenant/api/preview", async (c) => { const body = await c.req.json<{ text?: string }>(); const viewer = await currentViewer(c); if (!viewer) throw new HttpError(401, "sign in required"); limit(c, rl.preview, viewer); return c.json({ html: await c.get("store").preview({ text: body.text || "", viewer }) }); });
app.post("/:tenant/api/react", async (c) => { const body = await c.req.json<{ comment_id: string; content: string; on?: boolean }>(); const viewer = await currentViewer(c); limit(c, rl.react, viewer); return c.json(await c.get("store").react({ commentId: body.comment_id, content: body.content, on: body.on ?? true, viewer }) as object); });

app.onError((error, c) => {
  if (error instanceof HttpError) {
    const response = c.json({ detail: error.message }, error.status as 400);
    if (error.headers) for (const [name, value] of Object.entries(error.headers)) response.headers.set(name, value);
    return response;
  }
  console.error("unhandled error:", error);
  return c.json({ detail: "internal error" }, 500);
});

export default app;
