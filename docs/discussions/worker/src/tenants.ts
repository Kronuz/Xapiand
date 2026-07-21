import { HttpError, type Cfg } from "./config.js";
import type { Database } from "./db.js";
import { decryptTenantConfig, type TenantConfig } from "./tenant-config.js";

/** The one tenant selected by the first URL path segment. */
export class TenantContext {
  constructor(public id: string, public config: TenantConfig) {}

  isAdmin(tenantId: string, subject: string | null | undefined, login: string | null | undefined): boolean {
    return tenantId === this.id && this.config.moderators.some((moderator) =>
      moderator.subject ? moderator.subject === subject : Boolean(login) && moderator.login === login,
    );
  }
}

export async function loadTenant(db: Database, cfg: Cfg, tenantId: string, includeInactive = false): Promise<TenantContext> {
  const row = await db.tenantGet(tenantId);
  if (!row || !row.config_ciphertext || (!includeInactive && !row.active)) throw new HttpError(404, "tenant not found");
  const config = await decryptTenantConfig(row.config_ciphertext, row.config_nonce, cfg.configMasterKey);
  if (!includeInactive && !config.active) throw new HttpError(404, "tenant not found");
  return new TenantContext(tenantId, config);
}
