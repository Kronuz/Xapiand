import { HttpError } from "./config.js";
import type { TenantConfig } from "./tenant-config.js";

export interface OAuthUser {
  subject: string;
  login: string;
  avatarUrl: string;
  name: string;
  profileUrl: string;
}

function field(data: unknown, path: string): unknown {
  return path.split(".").reduce<unknown>((value, key) => value && typeof value === "object" ? (value as Record<string, unknown>)[key] : undefined, data);
}

function mapIdentity(oauth: TenantConfig["oauth"], data: unknown): OAuthUser {
  const subject = field(data, oauth.fields.subject);
  const login = field(data, oauth.fields.login);
  if ((typeof subject !== "string" && typeof subject !== "number") || typeof login !== "string") throw new HttpError(502, "OAuth identity response is missing required fields");
  const optional = (path: string): string => {
    const value = field(data, path);
    return typeof value === "string" ? value : "";
  };
  return { subject: String(subject), login, name: optional(oauth.fields.name) || login, avatarUrl: optional(oauth.fields.avatar), profileUrl: optional(oauth.fields.profileUrl) };
}

// GitHub OAuth App token introspection ("Check a token"): authenticated as the app with
// Basic client_id:client_secret, so unlike an authenticated userinfo call it is not blocked
// by an enterprise IP allow list. The returned authorization's `user` matches userinfo shape.
async function introspectToken(oauth: TenantConfig["oauth"], token: string): Promise<unknown> {
  const url = `${new URL(oauth.userUrl).origin}/applications/${oauth.clientId}/token`;
  const response = await fetch(url, {
    method: "POST",
    headers: { Authorization: "Basic " + btoa(`${oauth.clientId}:${oauth.clientSecret}`), Accept: "application/json", "User-Agent": "blog-comments" },
    body: JSON.stringify({ access_token: token }),
  });
  if (!response.ok) {
    const detail = await response.text().catch(() => "");
    console.warn(`identity introspection ${response.status} ${response.statusText} @ ${url}: ${detail.slice(0, 500)}`);
    throw new HttpError(502, `OAuth identity introspection failed (upstream ${response.status})`);
  }
  const data = await response.json<{ user?: unknown }>();
  return data.user;
}

export async function exchangeCode(oauth: TenantConfig["oauth"], code: string, redirectUri: string): Promise<string> {
  const params = new URLSearchParams({ code, redirect_uri: redirectUri });
  const headers: Record<string, string> = { Accept: "application/json", "Content-Type": "application/x-www-form-urlencoded", "User-Agent": "blog-comments" };
  if (oauth.clientAuthMethod === "client_secret_basic") {
    headers.Authorization = "Basic " + btoa(`${oauth.clientId}:${oauth.clientSecret}`);
  } else {
    params.set("client_id", oauth.clientId);
    params.set("client_secret", oauth.clientSecret);
  }
  const response = await fetch(oauth.tokenUrl, { method: "POST", headers, body: params });
  const data: Record<string, unknown> = await response.json<Record<string, unknown>>().catch(() => ({}));
  const token = data.access_token;
  if (!response.ok || typeof token !== "string" || !token) {
    console.warn(`token exchange ${response.status} @ ${oauth.tokenUrl}: ${String(data.error || "")} ${String(data.error_description || "")}`.trim());
    throw new HttpError(502, "OAuth token exchange failed");
  }
  return token;
}

export async function user(oauth: TenantConfig["oauth"], token: string): Promise<OAuthUser> {
  if (oauth.identitySource === "app-token") return mapIdentity(oauth, await introspectToken(oauth, token));
  const response = await fetch(oauth.userUrl, { headers: { Authorization: `Bearer ${token}`, Accept: "application/json", "User-Agent": "blog-comments" } });
  if (!response.ok) {
    const detail = await response.text().catch(() => "");
    console.warn(`identity lookup ${response.status} ${response.statusText} @ ${oauth.userUrl}: ${detail.slice(0, 500)}`);
    throw new HttpError(502, `OAuth identity lookup failed (upstream ${response.status})`);
  }
  return mapIdentity(oauth, await response.json<unknown>());
}
