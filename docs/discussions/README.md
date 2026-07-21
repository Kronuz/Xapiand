# Kronuz Discussions

A self-hosted, multi-tenant comment system for static sites. It includes a framework-free
browser widget, a ready-to-import Astro component, and a Cloudflare Worker backed by D1.

Each tenant has its own site metadata, allowed origins, OAuth client, moderators, limits,
notifications, optional private Atom feed, and optional static access capability. Comment
Markdown is rendered and sanitized by the Worker, then stored as HTML in D1.

## Features

- Threaded comments, replies, reactions, editing, deletion, and moderation.
- Generic OAuth 2.0 identity mapping, including separate OAuth clients per tenant.
- Public or capability-protected tenants selected by the URL path.
- Server-rendered Markdown with Shiki syntax highlighting.
- Discord, Slack, or Telegram notifications.
- Private per-tenant Atom feeds for recent comments.
- Local Astro development against either a local or production Worker.

## Layout

```text
astro/      ready-to-import Astro component
widget/     framework-free JavaScript and CSS widget
worker/     Hono Worker, D1 migrations, deployment scripts, tests, and operations docs
```

## Quick start

1. Follow [`worker/TUTORIAL.md`](worker/TUTORIAL.md) to create D1, configure the Worker,
   register an OAuth client, and add a tenant.
2. Import [`astro/Discussions.astro`](astro/Discussions.astro) in an Astro site, or embed
   the files from [`widget/`](widget/) in any HTML site.
3. Give the widget a stable page term and the complete tenant URL, such as
   `https://comments.example.com/my-blog`.

The reader session is tenant-scoped. The widget stores the bearer token in local storage
after OAuth so sign-in works when browsers block cross-site cookies, with the signed cookie
as a fallback.

Tenants with an empty `accessKey` are public. A non-empty key is a static capability sent
by the widget in `X-Discussions-Key`. It is suitable for a private static site whose readers
can already access the generated page, but it is not a replacement for individual reader
authorization.

## Documentation

- [`astro/README.md`](astro/README.md): Astro component integration.
- [`widget/README.md`](widget/README.md): standalone embedding, attributes, and theming.
- [`worker/README.md`](worker/README.md): architecture, configuration, and API summary.
- [`worker/TUTORIAL.md`](worker/TUTORIAL.md): first deployment and tenant setup.
- [`worker/OPERATIONS.md`](worker/OPERATIONS.md): D1 inspection, backup, recovery, and
  routine administration.

Deployment-specific values and tenant documents are intentionally gitignored. The tracked
tree is portable and should remain byte-identical wherever Kronuz Discussions is vendored.

Verify vendored copies while ignoring generated and deployment-local files:

```bash
./discussions/scripts/check-copies.sh ../another-site/discussions
```

The package manifest is currently `private` while this code is vendored in host repositories.
Before publishing it independently, choose a license, add repository metadata, remove
`private`, and publish the root package for the Astro/widget client. The Worker remains its
own deployable package under `worker/`.
