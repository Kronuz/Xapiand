# Deploying Kronuz Discussions

This tutorial creates one multi-tenant Cloudflare Worker and adds a blog tenant. Repeat the
tenant steps for every additional site. Each tenant gets a URL such as:

```text
https://comments.example.com/my-blog
```

The Worker stores comments in D1. Complete tenant documents, including OAuth client secrets,
are encrypted in D1 and managed with authenticated full-document `PUT` requests.

## 1. Install and configure the Worker

Requirements:

- Node.js and npm
- A Cloudflare account with Workers and D1
- An OAuth 2.0 client from the identity provider you want readers to use

Install dependencies and create deployment-local files:

```bash
npm install
cp wrangler.toml.example wrangler.toml
cp .dev.vars.example .dev.vars
cp tenant-config.example.json tenant-config.my-blog.json
```

`wrangler.toml`, `.dev.vars`, `secrets.sh`, and `tenant-config.*.json` are gitignored. The
sanitized `tenant-config.example.json` remains tracked.

Edit `wrangler.toml`:

- Set `name` and `database_name` if you do not want the defaults.
- Set `PUBLIC_BASE_URL` to the final Worker origin, without a tenant path.
- Leave `database_id = "local-dev-placeholder"` for a new D1 database. `deploy.sh` creates
  the database and writes its ID into the local file.

For local development, replace every `change-me` value in `.dev.vars`.

## 2. Understand the three deployment secrets

The Worker has only three deployment-wide secrets:

| Secret | Purpose |
| --- | --- |
| `SESSION_SECRET` | Signs OAuth state and tenant-bound sessions. |
| `CONFIG_MASTER_KEY` | Encrypts complete tenant configuration documents in D1. |
| `SERVICE_ADMIN_TOKEN` | Authorizes `PUT /:tenant/config`. |

Generate each value independently:

```bash
node --input-type=module -e 'import { randomBytes } from "node:crypto"; console.log(randomBytes(32).toString("base64url"))'
```

You may place the values in a gitignored `secrets.sh` before the first deployment:

```bash
SESSION_SECRET='...'
CONFIG_MASTER_KEY='...'
SERVICE_ADMIN_TOKEN='...'
```

`deploy.sh` preserves existing remote secrets. On a new service it generates missing values,
but you must save any generated `CONFIG_MASTER_KEY` and `SERVICE_ADMIN_TOKEN` immediately.
Losing the configuration master key makes stored tenant documents unrecoverable.

## 3. Create an OAuth client

Create an OAuth application in your identity provider. During initial setup, register this
callback URL, replacing the origin and tenant:

```text
https://comments.example.com/my-blog/auth/callback
```

For GitHub, OAuth Apps are under **Settings**, **Developer settings**, **OAuth Apps**. Use:

```json
{
  "name": "GitHub",
  "authorizeUrl": "https://github.com/login/oauth/authorize",
  "tokenUrl": "https://github.com/login/oauth/access_token",
  "userUrl": "https://api.github.com/user",
  "scope": "read:user",
  "clientAuthMethod": "client_secret_post",
  "identitySource": "userinfo",
  "fields": {
    "subject": "id",
    "login": "login",
    "name": "name",
    "avatar": "avatar_url",
    "profileUrl": "html_url"
  }
}
```

Other OAuth 2.0 providers work when their token response contains an access token and their
user endpoint returns JSON fields that can be selected by the configured field paths. Each
tenant may use a different provider or OAuth client.

The optional `identitySource` selects how the signed-in user is resolved after the token
exchange. The default `userinfo` calls `userUrl` with the user's access token. `app-token`
instead calls the OAuth app's token-introspection endpoint (for GitHub,
`POST /applications/{client_id}/token` at the `userUrl` origin) with the app's client
credentials and reads the returned `user`. Use `app-token` when the userinfo endpoint is
unreachable from the Worker, for example when an enterprise IP allow list blocks the Worker's
egress IP on the authenticated `userUrl` call: the app-credential introspection call is
authenticated as the app, not the user, so it is not IP-gated.

## 4. Complete the tenant document

Edit `tenant-config.my-blog.json`. Every update sends the entire document.

Important fields:

- `active`: set `false` to disable the tenant without deleting data.
- `site`: canonical site URL and repository metadata.
- `origins`: every browser origin allowed to call the tenant, including localhost aliases
  used during development.
- `oauth`: provider URLs, client credentials, scope, authentication method, and user-field
  mapping.
- `moderators`: stable OAuth subjects when available, with login as a fallback.
- `widget`: display-only options.
- `limits`: maximum comment body and session lifetime.
- `notifications`: webhook provider and destination.
- `feed`: optional private recent-comments Atom feed.

### Public and capability-protected tenants

Leave the top-level `accessKey` empty for a public tenant. For a protected tenant, generate
a 32-byte base64url value with the command from step 2. Compile that same value into the
private static site and pass it to the widget as `accessKey`.

This is a static site capability. Anyone who can read the generated page can inspect it. It
prevents public discovery and unauthenticated direct API access, but does not identify the
reader. OAuth authentication is still required to write comments.

### Private Atom feed

To enable the owner feed, set `feed.enabled` to `true` and generate a separate token. Never
reuse `accessKey` or a deployment secret. Subscribe with:

```text
https://comments.example.com/my-blog/comments/feed?token=<feed.token>
```

The feed token stays in the private tenant document and the feed reader. The static widget
does not need it.

## 5. Validate locally

Apply D1 migrations and start the local Worker:

```bash
npm run migrate:local
npm run dev
```

In another terminal, load the local administrator token and submit the tenant:

```bash
set -a
source .dev.vars
set +a

curl --fail-with-body -X PUT \
  http://127.0.0.1:8787/my-blog/config \
  -H "Authorization: Bearer $SERVICE_ADMIN_TOKEN" \
  -H 'Content-Type: application/json' \
  --data-binary @tenant-config.my-blog.json
```

Point Astro at the local tenant:

```bash
PUBLIC_DISCUSSIONS_BACKEND=http://127.0.0.1:8787/my-blog npm run dev
```

For a protected tenant, also set `PUBLIC_DISCUSSIONS_ACCESS_KEY`. The value becomes part of
the generated client page, so this environment variable is a build input rather than a
server secret.

You may instead use the production Worker from local Astro. Add both
`http://localhost:4321` and `http://127.0.0.1:4321` to the production tenant's `origins`.
OAuth starts and finishes at the Worker, then its signed state returns the reader to localhost.

## 6. Test and deploy

Run the local checks:

```bash
npm run typecheck
npm test
npx wrangler deploy --dry-run
```

Authenticate and deploy:

```bash
npx wrangler login
./deploy.sh
```

Load the production administrator token and submit the complete tenant document:

```bash
set -a
source secrets.sh
set +a

curl --fail-with-body -X PUT \
  https://comments.example.com/my-blog/config \
  -H "Authorization: Bearer $SERVICE_ADMIN_TOKEN" \
  -H 'Content-Type: application/json' \
  --data-binary @tenant-config.my-blog.json
```

`201 Created` means the tenant was created. `200 OK` means its complete configuration was
replaced. There is no delete API. Set `active` to `false` to take a tenant offline while
preserving its comments.

## 7. Add the Astro component

Import the component vendored with Kronuz Discussions:

```astro
---
import Discussions from '../discussions/astro/Discussions.astro';
---

<Discussions
  backend="https://comments.example.com/my-blog"
  term="blog/my-post"
  title="My post"
  url="https://blog.example.com/blog/my-post/"
/>
```

`term` is the stable page identity. Renaming it creates a new discussion. Tenant paths keep
identical terms from different blogs isolated.

For plain HTML or another framework, follow [`../widget/README.md`](../widget/README.md).

## 8. Verify production

Check health and the public configuration projection:

```bash
curl --fail https://comments.example.com/health
curl --fail https://comments.example.com/my-blog/config
```

For a protected tenant, include the site capability:

```bash
curl --fail \
  -H "X-Discussions-Key: $(jq -r '.accessKey' tenant-config.my-blog.json)" \
  https://comments.example.com/my-blog/config
```

Then test in the browser:

1. Confirm comments load on an existing page.
2. Sign in through the configured OAuth provider.
3. Post, edit, react to, and delete a comment.
4. Verify moderator actions with a configured moderator account.
5. Verify the notification hook and private feed when enabled.
6. Repeat from each registered localhost origin.

## Adding or changing a tenant

There is no separate database provisioning step per tenant:

1. Choose a URL-safe tenant ID.
2. Register `<PUBLIC_BASE_URL>/<tenant>/auth/callback` with its OAuth client.
3. Copy `tenant-config.example.json` to a gitignored tenant file.
4. Fill every field and generate distinct optional access and feed tokens.
5. Send an authenticated full-document `PUT /<tenant>/config`.
6. Point the site at `<PUBLIC_BASE_URL>/<tenant>`.
7. Verify production and localhost flows.

For routine D1 queries, backups, migrations, logs, and recovery, continue with
[`OPERATIONS.md`](OPERATIONS.md).
