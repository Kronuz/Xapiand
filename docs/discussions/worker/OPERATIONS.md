# Worker and D1 operations handbook

This is the day-to-day handbook for inspecting, maintaining, and troubleshooting the
deployed comments service. For initial deployment, OAuth setup, and adding tenants, use
[TUTORIAL.md](./TUTORIAL.md).

Run every command below from `discussions/worker/`.

## The important safety rule

Wrangler can address either a local development database or the production D1 database.
Always spell out the target:

```bash
# Local Miniflare database under .wrangler/
npx wrangler d1 execute discussions --local --command "SELECT COUNT(*) FROM comments;"

# Production Cloudflare D1 database
npx wrangler d1 execute discussions --remote --command "SELECT COUNT(*) FROM comments;"
```

Do not rely on a default when working with data. Start with `SELECT`, export production
before a repair, and use the service API instead of direct SQL whenever an API operation
exists.

Authenticate Wrangler if a remote command asks for it:

```bash
npx wrangler login
npx wrangler whoami
```

## Quick service checks

Check that the Worker is running and that each active tenant exposes its redacted public
configuration:

```bash
curl --fail https://comments.example.com/health
curl --fail https://comments.example.com/my-blog/config
curl --fail \
  -H "X-Discussions-Key: $(jq -r '.accessKey' tenant-config.private-blog.json)" \
  https://comments.example.com/private-blog/config
```

A tenant config response must not contain its OAuth client secret, webhook URL, feed
token, allowed origins, or moderator list.

Check that OAuth starts with a redirect instead of following it:

```bash
curl --silent --show-error --head \
  https://comments.example.com/my-blog/auth/login
```

The `Location` header should name the configured OAuth authorization endpoint and contain
the expected client ID and tenant callback URL.

## Understand the stored data

The principal tables are:

| table | contents |
| --- | --- |
| `tenants` | tenant identity plus its encrypted complete configuration |
| `tenant_admins` | materialized moderator logins for each tenant |
| `discussions` | one page or thread per `(tenant_id, term)` |
| `comments` | Markdown, rendered HTML, authorship, and reply relationships |
| `reactions` | one row per comment, user, and emoji |
| `d1_migrations` | migrations already applied by Wrangler |

Tenant configuration ciphertext is private. Comment bodies may also contain information
that should not be copied into tickets, terminals being recorded, or public logs. The
routine queries below deliberately inspect metadata rather than either value.

## Useful read-only D1 queries

### List tables and schema

```bash
npx wrangler d1 execute discussions --remote --command \
  "SELECT name FROM sqlite_schema WHERE type = 'table' ORDER BY name;"

npx wrangler d1 execute discussions --remote --command \
  "PRAGMA table_info(comments);"

npx wrangler d1 execute discussions --remote --command \
  "PRAGMA index_list(comments);"
```

### Inspect migration state

Use Wrangler's view first:

```bash
npx wrangler d1 migrations list discussions --remote
```

The underlying records can also be inspected directly:

```bash
npx wrangler d1 execute discussions --remote --command \
  "SELECT id, name, applied_at FROM d1_migrations ORDER BY id;"
```

### Inspect tenants without revealing configuration

```bash
npx wrangler d1 execute discussions --remote --command \
  "SELECT id, active, datetime(created_at, 'unixepoch') AS created, datetime(updated_at, 'unixepoch') AS updated, length(config_ciphertext) AS encrypted_bytes, length(config_nonce) AS nonce_bytes FROM tenants ORDER BY id;"
```

An active tenant should have nonzero `encrypted_bytes` and `nonce_bytes`. Do not select or
copy `config_ciphertext` or `config_nonce`; manage the complete document through
`PUT /:tenant/config`.

### Count data by tenant

```bash
npx wrangler d1 execute discussions --remote --command \
  "SELECT tenant_id, COUNT(*) AS discussions FROM discussions GROUP BY tenant_id ORDER BY tenant_id;"

npx wrangler d1 execute discussions --remote --command \
  "SELECT tenant_id, COUNT(*) AS comments FROM comments GROUP BY tenant_id ORDER BY tenant_id;"

npx wrangler d1 execute discussions --remote --command \
  "SELECT tenant_id, COUNT(*) AS reactions FROM reactions GROUP BY tenant_id ORDER BY tenant_id;"
```

Tenants with zero rows do not appear in a grouped result. To include them:

```bash
npx wrangler d1 execute discussions --remote --command \
  "SELECT t.id, COUNT(c.id) AS comments FROM tenants AS t LEFT JOIN comments AS c ON c.tenant_id = t.id GROUP BY t.id ORDER BY t.id;"
```

### Find active threads and recent comments

```bash
npx wrangler d1 execute discussions --remote --command \
  "SELECT tenant_id, term, COUNT(*) AS comments, datetime(MAX(created_at), 'unixepoch') AS latest FROM comments GROUP BY tenant_id, term ORDER BY MAX(created_at) DESC LIMIT 25;"

npx wrangler d1 execute discussions --remote --command \
  "SELECT id, tenant_id, term, parent_id, author_login, datetime(created_at, 'unixepoch') AS created, is_minimized, hidden_at IS NOT NULL AS hidden FROM comments ORDER BY created_at DESC LIMIT 25;"
```

These queries omit `body_md` and `body_html`. Select those columns only when the content
itself is necessary to investigate a specific report.

### Run integrity checks

Every healthy query below should return zero rows.

Comments whose tenant no longer exists:

```bash
npx wrangler d1 execute discussions --remote --command \
  "SELECT c.id, c.tenant_id FROM comments AS c LEFT JOIN tenants AS t ON t.id = c.tenant_id WHERE t.id IS NULL;"
```

Comments whose discussion is missing:

```bash
npx wrangler d1 execute discussions --remote --command \
  "SELECT c.id, c.tenant_id, c.term FROM comments AS c LEFT JOIN discussions AS d ON d.tenant_id = c.tenant_id AND d.term = c.term WHERE d.term IS NULL;"
```

Replies whose parent is missing or belongs to another tenant or thread:

```bash
npx wrangler d1 execute discussions --remote --command \
  "SELECT c.id, c.tenant_id, c.term, c.parent_id FROM comments AS c LEFT JOIN comments AS p ON p.id = c.parent_id AND p.tenant_id = c.tenant_id AND p.term = c.term WHERE c.parent_id IS NOT NULL AND p.id IS NULL;"
```

Reactions whose comment is missing or belongs to another tenant:

```bash
npx wrangler d1 execute discussions --remote --command \
  "SELECT r.comment_id, r.tenant_id, r.login, r.content FROM reactions AS r LEFT JOIN comments AS c ON c.id = r.comment_id AND c.tenant_id = r.tenant_id WHERE c.id IS NULL;"
```

Tenant administrators whose tenant is missing:

```bash
npx wrangler d1 execute discussions --remote --command \
  "SELECT a.tenant_id, a.login FROM tenant_admins AS a LEFT JOIN tenants AS t ON t.id = a.tenant_id WHERE t.id IS NULL;"
```

## Local D1 development

Apply all migrations to the local database, then start the Worker:

```bash
npm run migrate:local
npm run dev
```

Local D1 data is kept below `.wrangler/` and is gitignored. Any read-only production query
in this document can be tested locally by replacing `--remote` with `--local`.

To submit a local tenant, fill a private tenant JSON and use the service admin token from
`.dev.vars`:

```bash
curl --fail-with-body -X PUT http://localhost:8787/my-blog/config \
  -H 'Authorization: Bearer dev-local-service-admin-token-change-me' \
  -H 'Content-Type: application/json' \
  --data-binary @tenant-config.my-blog.json
```

The OAuth App callback still needs to match the Worker receiving the callback. The normal
Astro `npm run dev` workflow instead talks to the production Worker, as explained in
[TUTORIAL.md](./TUTORIAL.md).

## Create and apply migrations

Create the next numbered SQL file through Wrangler:

```bash
npx wrangler d1 migrations create discussions describe_the_change
```

Edit the generated file, then exercise it locally:

```bash
npx wrangler d1 migrations list discussions --local
npx wrangler d1 migrations apply discussions --local
npm test
npm run typecheck
```

Before applying it remotely, export production and inspect the pending list:

```bash
mkdir -p backups
npx wrangler d1 export discussions --remote \
  --output=backups/discussions-before-migration.sql
npx wrangler d1 migrations list discussions --remote
npx wrangler d1 migrations apply discussions --remote
```

Afterward, rerun the migration list, service checks, tenant counts, and relevant integrity
queries. Never rewrite a migration that has already reached production. Add a new
migration that moves the existing schema forward.

## Export and restore data

Create a full SQL export:

```bash
mkdir -p backups
npx wrangler d1 export discussions --remote \
  --output=backups/discussions-$(date +%Y-%m-%d).sql
```

The `backups/` directory is gitignored, but the files still contain private tenant
ciphertext and public comment content. Store them with the same care as other production
backups. Cloudflare recommends exporting during a quieter period because export work can
temporarily affect database availability.

Useful narrower exports include:

```bash
# Schema only
npx wrangler d1 export discussions --remote --no-data \
  --output=backups/discussions-schema.sql

# Selected tables
npx wrangler d1 export discussions --remote \
  --table=discussions --table=comments --table=reactions \
  --output=backups/discussions-content.sql
```

Import an SQL file into a local database for rehearsal:

```bash
npx wrangler d1 execute discussions --local \
  --file=backups/discussions-content.sql
```

Do not import a full export over production as a routine rollback. D1 Time Travel is the
safer production recovery mechanism when its retention window covers the incident.

## D1 Time Travel

Ask D1 for the current bookmark or the bookmark corresponding to an RFC 3339 timestamp:

```bash
npx wrangler d1 time-travel info discussions
npx wrangler d1 time-travel info discussions \
  --timestamp=2026-07-20T12:00:00Z
```

Restoring changes production data. First confirm the timestamp, save the returned
bookmark, export the current database, and inspect the proposed recovery point. Then, only
when recovery is genuinely intended:

```bash
npx wrangler d1 time-travel restore discussions \
  --bookmark=<confirmed-bookmark>
```

Time Travel is a database operation. Rolling back a Worker deployment does not roll back
its D1 schema or data.

## Logs, deployments, and Worker rollback

Stream production request logs:

```bash
npx wrangler tail discussions --format pretty
```

Show only failed requests:

```bash
npx wrangler tail discussions --format pretty --status error
```

List recent Worker deployments:

```bash
npx wrangler deployments list --name discussions
```

If application code must be rolled back, choose a known version from that list:

```bash
npx wrangler rollback <version-id> --name discussions \
  --message "Reason for rollback"
```

Confirm that the older code is compatible with the current D1 schema before rolling back.
Worker rollback does not reverse migrations.

## Manage tenant configuration

The complete gitignored JSON is the editable source of truth. Read the redacted public
projection:

```bash
curl --fail https://comments.example.com/my-blog/config
```

Create or completely replace the private configuration:

```bash
set -a
source ./secrets.sh
set +a

curl --fail-with-body -X PUT \
  https://comments.example.com/my-blog/config \
  -H "Authorization: Bearer $SERVICE_ADMIN_TOKEN" \
  -H 'Content-Type: application/json' \
  --data-binary @tenant-config.my-blog.json
```

There is no partial update and no tenant deletion. To disable a tenant, change its full
document to `"active": false` and send the same `PUT`. Do not update only the `active`
database column because the encrypted document is also authoritative.

## Manage Worker secrets

List secret names without revealing values:

```bash
npx wrangler secret list
```

Create or replace one through Wrangler's prompt:

```bash
npx wrangler secret put SESSION_SECRET
```

Delete an obsolete binding only after verifying that neither code nor `wrangler.toml`
uses it:

```bash
npx wrangler secret delete OBSOLETE_SECRET
```

Cloudflare creates a new Worker version when a secret is changed or deleted. The service
must retain `SESSION_SECRET`, `CONFIG_MASTER_KEY`, and `SERVICE_ADMIN_TOKEN`. Losing or
replacing `CONFIG_MASTER_KEY` without re-encrypting every tenant makes their stored
configuration unreadable.

## Repair policy

For a suspected data problem:

1. Capture the failing URL, tenant, thread term, UTC time, and request result.
2. Check `/health`, public tenant config, and `wrangler tail`.
3. Run a narrowly scoped `SELECT` and the relevant integrity check.
4. Export D1 before any write.
5. Prefer a service API operation or a tested migration.
6. If direct SQL is unavoidable, rehearse it against local data and select the exact rows
   again immediately before the write.
7. Verify counts and integrity after the repair.

Avoid direct `DELETE` examples in an operations transcript. Comments and tenants have
related rows, and D1 does not infer the service's tenant invariants from foreign keys in
this schema.

## Common failures

| symptom | first checks |
| --- | --- |
| `/health` fails | deployment list, `wrangler tail`, Worker route and account |
| `/:tenant/config` returns `404` | tenant spelling, `active`, access key, encrypted config lengths |
| tenant config returns `500` after a secret change | `CONFIG_MASTER_KEY` continuity, Worker logs |
| OAuth provider rejects callback | exact OAuth App callback, tenant `oauth.callbackUrl`, Worker base URL |
| browser reports CORS failure | exact scheme and host in tenant `origins`, including localhost alias |
| login succeeds but API returns `401` | tenant path consistency, tenant-scoped cookie, session-secret continuity |
| comments appear on the wrong page | widget term, tenant backend URL, discussion and comment tenant IDs |
| webhook or feed fails | complete private tenant JSON, hook response in `wrangler tail`, feed token |
| older Worker fails after rollback | code compatibility with migrations that remain applied |

## Cloudflare references

- [D1 Wrangler commands](https://developers.cloudflare.com/d1/wrangler-commands/)
- [Import and export data](https://developers.cloudflare.com/d1/best-practices/import-export-data/)
- [D1 migrations](https://developers.cloudflare.com/d1/reference/migrations/)
- [D1 Time Travel](https://developers.cloudflare.com/d1/reference/time-travel/)
- [Worker secrets](https://developers.cloudflare.com/workers/configuration/secrets/)
- [Worker versions and deployments](https://developers.cloudflare.com/workers/versions-and-deployments/)
- [Worker rollbacks](https://developers.cloudflare.com/workers/versions-and-deployments/rollbacks/)
