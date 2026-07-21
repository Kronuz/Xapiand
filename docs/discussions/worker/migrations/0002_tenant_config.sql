-- Tenant configuration is a single encrypted document. Existing comment data remains
-- keyed by tenant_id and is deliberately untouched by this migration.
ALTER TABLE tenants ADD COLUMN active INTEGER NOT NULL DEFAULT 0;
ALTER TABLE tenants ADD COLUMN config_ciphertext TEXT NOT NULL DEFAULT '';
ALTER TABLE tenants ADD COLUMN config_nonce TEXT NOT NULL DEFAULT '';
ALTER TABLE tenants ADD COLUMN updated_at REAL;
