# Kronuz Discussions Worker

Multi-tenant blog comments on Cloudflare Workers, Hono, and D1. A deployment can
serve any number of blogs. The tenant is the first URL segment:

```text
https://comments.example.com/my-blog
```

For first deployment, OAuth setup, verification, and future tenant procedures, follow
[TUTORIAL.md](./TUTORIAL.md).

For routine D1 inspection, backups, migrations, logs, tenant administration, recovery,
and troubleshooting, use [OPERATIONS.md](./OPERATIONS.md).

Every tenant owns its complete site, origin, OAuth, moderator, widget, limit,
notification, and feed configuration. Reading is public when `accessKey` is empty and
capability-protected otherwise. Authentication is required to post, react, edit, or
moderate.

An empty top-level `accessKey` makes a tenant public. A 32-byte base64url value protects
all browser-facing tenant operations behind the `X-Discussions-Key` header. The key is a
static site capability, not per-reader authorization.

## Configuration model

Management has two operations:

```text
GET /:tenant/config   public widget configuration
PUT /:tenant/config   authenticated upsert of the complete private configuration
```

There is no delete operation. Set `active` to `false` in a full `PUT` to take a tenant
offline without removing its discussions, comments, or reactions. A later full `PUT` with
`active: true` restores it.

The complete configuration is AES-GCM encrypted in D1 with `CONFIG_MASTER_KEY`. Public
`GET` returns a strict projection and never exposes OAuth credentials, webhook URLs, feed
tokens, moderators, or origins. See `tenant-config.example.json` for every option.

Each tenant's `oauth.identitySource` selects how the signed-in user is resolved: the default
`userinfo` reads the provider's `userUrl` with the user token, while `app-token` uses the
OAuth app's token-introspection endpoint with the app credentials. The latter is for
providers that block the Worker's IP on the authenticated userinfo call (for example an
enterprise IP allow list), because the introspection call is authenticated as the app and is
not IP-gated.

Only deployment-wide infrastructure stays outside tenant configuration:

| binding | purpose |
| --- | --- |
| `DB` | D1 database |
| `PUBLIC_BASE_URL` | Worker origin, without a tenant path |
| `REQUEST_MAX_BYTES` | hard service request-size ceiling |
| `SESSION_SECRET` | signs tenant-bound sessions and OAuth state |
| `CONFIG_MASTER_KEY` | encrypts complete tenant documents in D1 |
| `SERVICE_ADMIN_TOKEN` | authorizes `PUT /:tenant/config` |

The last three are Worker secrets. Losing `CONFIG_MASTER_KEY` makes stored tenant
configuration unrecoverable. Back it up securely.

For deployment, they may be placed in a gitignored `secrets.sh`. An empty
`SESSION_SECRET` preserves an existing remote value; on a fresh deployment, `deploy.sh`
generates one. Non-empty `CONFIG_MASTER_KEY` and `SERVICE_ADMIN_TOKEN` values are used
when those remote secrets do not exist yet.

## Local development

```bash
npm install
cp wrangler.toml.example wrangler.toml
cp .dev.vars.example .dev.vars
npm run migrate:local
npm run dev
```

Create a tenant in the local Worker:

```bash
cp tenant-config.example.json tenant-config.json
# Fill every value, including OAuth credentials.
curl -X PUT http://localhost:8787/my-blog/config \
  -H 'Authorization: Bearer dev-local-service-admin-token-change-me' \
  -H 'Content-Type: application/json' \
  --data-binary @tenant-config.json
```

For the usual blog workflow, Astro continues using the production Worker and production
comments during `npm run dev`. Register both `http://localhost:4321` and
`http://127.0.0.1:4321` in that production tenant's `origins` array. OAuth still returns
to the production callback, then the signed state sends the reader back to localhost.

## Production deployment

```bash
npx wrangler login
cp wrangler.toml.example wrangler.toml
# Set PUBLIC_BASE_URL and, for an existing D1 database, database_id.
./deploy.sh
```

After deployment, copy and fill `tenant-config.example.json`, then send the full document:

```bash
curl -X PUT https://comments.example.com/my-blog/config \
  -H 'Authorization: Bearer <SERVICE_ADMIN_TOKEN>' \
  -H 'Content-Type: application/json' \
  --data-binary @tenant-config.json
```

Register this callback with that tenant's OAuth client:

```text
https://comments.example.com/my-blog/auth/callback
```

The blog needs only the tenant base URL:

```ts
export const DISCUSSIONS_BACKEND =
  "https://comments.example.com/my-blog";
```

## Tenant API

```text
GET  /:tenant/config
PUT  /:tenant/config

GET  /:tenant/auth/login
POST /:tenant/auth/login
GET  /:tenant/auth/callback
POST /:tenant/auth/logout

GET  /:tenant/api/me
GET  /:tenant/api/discussions
POST /:tenant/api/comments
POST /:tenant/api/comments/edit
POST /:tenant/api/comments/delete
POST /:tenant/api/comments/hide
POST /:tenant/api/preview
POST /:tenant/api/react

GET  /:tenant/comments/feed?token=...
GET  /health
```

The path selects a tenant. For browser API and login requests, the request or return
origin must also appear in that tenant's complete configuration. Sessions, cookies, rate
limits, and browser local-storage keys are tenant-scoped.

## Verification

```bash
npm run typecheck
npm test
npx wrangler deploy --dry-run
```
