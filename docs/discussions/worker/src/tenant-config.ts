import { HttpError, type Env } from "./config.js";
import { isAccessKey } from "./access.js";

export interface TenantConfig {
  active: boolean;
  accessKey: string;
  site: { url: string; repo: string; repoUrl: string };
  origins: string[];
  oauth: {
    name: string;
    authorizeUrl: string;
    tokenUrl: string;
    userUrl: string;
    clientId: string;
    clientSecret: string;
    scope: string;
    clientAuthMethod: "client_secret_post" | "client_secret_basic";
    identitySource: "userinfo" | "app-token";
    fields: { subject: string; login: string; name: string; avatar: string; profileUrl: string };
  };
  moderators: Array<{ subject?: string; login: string }>;
  widget: { stripSuffix: string; giphyKey: string };
  limits: { maxBody: number; sessionTtl: number };
  notifications: { kind: string; webhookUrl: string; telegramChat: string };
  feed: { enabled: boolean; token: string };
}

export interface PublicTenantConfig {
  siteUrl: string;
  repo: string;
  repoUrl: string;
  oauth: { enabled: boolean; name: string };
  widget: TenantConfig["widget"];
  limits: { maxBody: number };
}

const encoder = new TextEncoder();
const decoder = new TextDecoder();
const TENANT_RE = /^[a-z0-9][a-z0-9-]{0,62}$/;

function bytesToB64(bytes: Uint8Array): string {
  let out = "";
  for (const byte of bytes) out += String.fromCharCode(byte);
  return btoa(out);
}

function b64ToBytes(value: string): Uint8Array {
  const raw = atob(value);
  return Uint8Array.from(raw, (c) => c.charCodeAt(0));
}

async function encryptionKey(secret: string): Promise<CryptoKey> {
  if (!secret) throw new HttpError(503, "CONFIG_MASTER_KEY is not configured");
  const digest = await crypto.subtle.digest("SHA-256", encoder.encode(secret));
  return crypto.subtle.importKey("raw", digest, "AES-GCM", false, ["encrypt", "decrypt"]);
}

export function validateTenantId(id: string): void {
  if (!TENANT_RE.test(id)) throw new HttpError(404, "tenant not found");
}

function object(value: unknown, name: string): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new HttpError(400, `${name} must be an object`);
  return value as Record<string, unknown>;
}

function string(value: unknown, name: string, allowEmpty = true): string {
  if (typeof value !== "string" || (!allowEmpty && !value)) throw new HttpError(400, `${name} must be a string`);
  return value;
}

function positiveInt(value: unknown, name: string, ceiling: number): number {
  if (!Number.isInteger(value) || Number(value) <= 0 || Number(value) > ceiling) {
    throw new HttpError(400, `${name} must be an integer from 1 to ${ceiling}`);
  }
  return Number(value);
}

function endpoint(value: unknown, name: string): string {
  const text = string(value, name, false);
  let url: URL;
  try { url = new URL(text); } catch { throw new HttpError(400, `${name} must be an absolute URL`); }
  if (url.protocol !== "https:" && url.hostname !== "localhost" && url.hostname !== "127.0.0.1") {
    throw new HttpError(400, `${name} must use HTTPS`);
  }
  return url.toString().replace(/\/$/, "");
}

function origin(value: unknown, name: string): string {
  const text = string(value, name, false);
  let url: URL;
  try { url = new URL(text); } catch { throw new HttpError(400, `${name} must be an origin`); }
  if (url.origin !== text.replace(/\/$/, "") || (url.protocol !== "https:" && url.hostname !== "localhost" && url.hostname !== "127.0.0.1")) {
    throw new HttpError(400, `${name} must be an HTTPS origin (localhost may use HTTP)`);
  }
  return url.origin;
}

export function validateTenantConfig(input: unknown, env: Env): TenantConfig {
  const root = object(input, "config");
  const site = object(root.site, "site");
  const oauth = object(root.oauth, "oauth");
  const fields = object(oauth.fields, "oauth.fields");
  const widget = object(root.widget, "widget");
  const limits = object(root.limits, "limits");
  const notifications = object(root.notifications, "notifications");
  const feed = object(root.feed, "feed");
  if (typeof root.active !== "boolean") throw new HttpError(400, "active must be a boolean");
  const accessKey = string(root.accessKey, "accessKey");
  if (!isAccessKey(accessKey)) {
    throw new HttpError(400, "accessKey must be empty or a 32-byte base64url value");
  }
  if (!Array.isArray(root.origins) || root.origins.length === 0) throw new HttpError(400, "origins must be a non-empty array");
  if (!Array.isArray(root.moderators)) throw new HttpError(400, "moderators must be an array");
  const authMethod = string(oauth.clientAuthMethod, "oauth.clientAuthMethod");
  if (authMethod !== "client_secret_post" && authMethod !== "client_secret_basic") {
    throw new HttpError(400, "oauth.clientAuthMethod is invalid");
  }
  const identitySource = oauth.identitySource === undefined ? "userinfo" : string(oauth.identitySource, "oauth.identitySource");
  if (identitySource !== "userinfo" && identitySource !== "app-token") {
    throw new HttpError(400, "oauth.identitySource is invalid");
  }
  const maxBodyCeiling = Number(env.REQUEST_MAX_BYTES || 1_048_576);
  return {
    active: root.active,
    accessKey,
    site: {
      url: origin(site.url, "site.url"),
      repo: string(site.repo, "site.repo"),
      repoUrl: endpoint(site.repoUrl, "site.repoUrl"),
    },
    origins: [...new Set(root.origins.map((v, i) => origin(v, `origins[${i}]`)))],
    oauth: {
      name: string(oauth.name, "oauth.name", false),
      authorizeUrl: endpoint(oauth.authorizeUrl, "oauth.authorizeUrl"),
      tokenUrl: endpoint(oauth.tokenUrl, "oauth.tokenUrl"),
      userUrl: endpoint(oauth.userUrl, "oauth.userUrl"),
      clientId: string(oauth.clientId, "oauth.clientId", false),
      clientSecret: string(oauth.clientSecret, "oauth.clientSecret", false),
      scope: string(oauth.scope, "oauth.scope"),
      clientAuthMethod: authMethod,
      identitySource,
      fields: {
        subject: string(fields.subject, "oauth.fields.subject", false),
        login: string(fields.login, "oauth.fields.login", false),
        name: string(fields.name, "oauth.fields.name", false),
        avatar: string(fields.avatar, "oauth.fields.avatar", false),
        profileUrl: string(fields.profileUrl, "oauth.fields.profileUrl", false),
      },
    },
    moderators: root.moderators.map((value, i) => {
      const moderator = object(value, `moderators[${i}]`);
      return { subject: moderator.subject ? string(moderator.subject, `moderators[${i}].subject`) : undefined, login: string(moderator.login, `moderators[${i}].login`, false) };
    }),
    widget: { stripSuffix: string(widget.stripSuffix, "widget.stripSuffix"), giphyKey: string(widget.giphyKey, "widget.giphyKey") },
    limits: {
      maxBody: positiveInt(limits.maxBody, "limits.maxBody", maxBodyCeiling),
      sessionTtl: positiveInt(limits.sessionTtl, "limits.sessionTtl", 31_536_000),
    },
    notifications: {
      kind: string(notifications.kind, "notifications.kind"),
      webhookUrl: string(notifications.webhookUrl, "notifications.webhookUrl"),
      telegramChat: string(notifications.telegramChat, "notifications.telegramChat"),
    },
    feed: { enabled: Boolean(feed.enabled), token: string(feed.token, "feed.token") },
  };
}

export async function encryptTenantConfig(config: TenantConfig, secret: string): Promise<{ ciphertext: string; nonce: string }> {
  const nonce = crypto.getRandomValues(new Uint8Array(12));
  const encrypted = await crypto.subtle.encrypt({ name: "AES-GCM", iv: nonce }, await encryptionKey(secret), encoder.encode(JSON.stringify(config)));
  return { ciphertext: bytesToB64(new Uint8Array(encrypted)), nonce: bytesToB64(nonce) };
}

export async function decryptTenantConfig(ciphertext: string, nonce: string, secret: string): Promise<TenantConfig> {
  try {
    const decrypted = await crypto.subtle.decrypt({ name: "AES-GCM", iv: b64ToBytes(nonce) }, await encryptionKey(secret), b64ToBytes(ciphertext));
    return JSON.parse(decoder.decode(decrypted)) as TenantConfig;
  } catch {
    throw new HttpError(500, "tenant configuration could not be decrypted");
  }
}

export function publicTenantConfig(config: TenantConfig): PublicTenantConfig {
  return {
    siteUrl: config.site.url,
    repo: config.site.repo,
    repoUrl: config.site.repoUrl,
    oauth: { enabled: Boolean(config.oauth.clientId && config.oauth.clientSecret), name: config.oauth.name },
    widget: config.widget,
    limits: { maxBody: config.limits.maxBody },
  };
}
