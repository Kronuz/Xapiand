/**
 * Stateless session store: the cookie value *is* the session, HMAC-signed with WebCrypto.
 *
 * There is no server state,
 * so it works across edge isolates and scale-to-zero with no `sessions` table and no
 * sweeper. The Worker never stores the reader's OAuth token after login. It keeps only
 * verified identity, so no provider secret rides
 * in the cookie. Signing (not encryption) is therefore enough.
 */
export interface Identity {
  tenantId: string;
  subject: string;
  login: string;
  name: string;
  avatarUrl: string;
  url?: string | null;
}

const enc = new TextEncoder();
const dec = new TextDecoder();

async function hmacKey(secret: string): Promise<CryptoKey> {
  return crypto.subtle.importKey("raw", enc.encode(secret), { name: "HMAC", hash: "SHA-256" }, false, [
    "sign",
    "verify",
  ]);
}

function b64urlFromBytes(bytes: Uint8Array): string {
  let s = "";
  for (const b of bytes) s += String.fromCharCode(b);
  return btoa(s).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
}

function bytesFromB64url(input: string): Uint8Array {
  let s = input.replace(/-/g, "+").replace(/_/g, "/");
  while (s.length % 4) s += "=";
  const bin = atob(s);
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
  return out;
}

export class CookieSessionStore {
  constructor(
    private secret: string,
    private ttl: number,
  ) {}

  /** Mint the opaque cookie value for a signed-in reader's identity. */
  async create(data: Identity): Promise<string> {
    const body = JSON.stringify({ d: data, exp: Date.now() / 1000 + this.ttl });
    const payload = enc.encode(body);
    const key = await hmacKey(this.secret);
    const sig = new Uint8Array(await crypto.subtle.sign("HMAC", key, payload));
    return b64urlFromBytes(payload) + "." + b64urlFromBytes(sig);
  }

  /** Resolve a cookie value back to the identity, or null if bad/expired/tampered. */
  async get(value: string): Promise<Identity | null> {
    const dot = value.lastIndexOf(".");
    if (dot < 0) return null;
    let payload: Uint8Array;
    let sig: Uint8Array;
    try {
      payload = bytesFromB64url(value.slice(0, dot));
      sig = bytesFromB64url(value.slice(dot + 1));
    } catch {
      return null;
    }
    const key = await hmacKey(this.secret);
    const ok = await crypto.subtle.verify("HMAC", key, sig, payload);
    if (!ok) return null;
    let body: { d?: Identity; exp?: number };
    try {
      body = JSON.parse(dec.decode(payload));
    } catch {
      return null;
    }
    if (!body || typeof body.exp !== "number" || body.exp <= Date.now() / 1000) return null;
    return body.d ?? null;
  }
}
