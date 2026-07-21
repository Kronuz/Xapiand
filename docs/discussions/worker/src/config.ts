import type { D1Database } from "@cloudflare/workers-types";

/** Deployment-wide infrastructure. Every blog-specific option lives in its encrypted
 * tenant configuration document in D1. */
export interface Env {
  DB: D1Database;
  PUBLIC_BASE_URL: string;
  SESSION_SECRET?: string;
  CONFIG_MASTER_KEY?: string;
  SERVICE_ADMIN_TOKEN?: string;
  REQUEST_MAX_BYTES?: string;
}

export interface Cfg {
  publicBaseUrl: string;
  sessionSecret: string;
  configMasterKey: string;
  serviceAdminToken: string;
  requestMaxBytes: number;
}

let warned = false;

export function loadCfg(env: Env): Cfg {
  if ((!env.SESSION_SECRET || !env.CONFIG_MASTER_KEY || !env.SERVICE_ADMIN_TOKEN) && !warned) {
    warned = true;
    console.warn("One or more Worker secrets are missing; tenant management or login will be unavailable.");
  }
  return {
    publicBaseUrl: (env.PUBLIC_BASE_URL || "http://127.0.0.1:8787").replace(/\/$/, ""),
    sessionSecret: env.SESSION_SECRET || "dev-insecure-session-secret-change-me",
    configMasterKey: env.CONFIG_MASTER_KEY || "",
    serviceAdminToken: env.SERVICE_ADMIN_TOKEN || "",
    requestMaxBytes: Number(env.REQUEST_MAX_BYTES || 1_048_576),
  };
}

export class HttpError extends Error {
  status: number;
  headers?: Record<string, string>;
  constructor(status: number, detail: string, headers?: Record<string, string>) {
    super(detail);
    this.status = status;
    this.headers = headers;
  }
}
