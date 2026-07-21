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
  if (!response.ok || typeof token !== "string" || !token) throw new HttpError(502, "OAuth token exchange failed");
  return token;
}

export async function user(oauth: TenantConfig["oauth"], token: string): Promise<OAuthUser> {
  const response = await fetch(oauth.userUrl, { headers: { Authorization: `Bearer ${token}`, Accept: "application/json", "User-Agent": "blog-comments" } });
  if (!response.ok) throw new HttpError(502, "OAuth identity lookup failed");
  const data = await response.json<unknown>();
  const subject = field(data, oauth.fields.subject);
  const login = field(data, oauth.fields.login);
  if ((typeof subject !== "string" && typeof subject !== "number") || typeof login !== "string") throw new HttpError(502, "OAuth identity response is missing required fields");
  const optional = (path: string): string => {
    const value = field(data, path);
    return typeof value === "string" ? value : "";
  };
  return { subject: String(subject), login, name: optional(oauth.fields.name) || login, avatarUrl: optional(oauth.fields.avatar), profileUrl: optional(oauth.fields.profileUrl) };
}
