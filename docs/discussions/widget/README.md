# Discussions widget

The front-end half of the self-hosted discussions system: a small, framework-free
`discussions.css` + `discussions.js` that render a page's comments in a threaded
timeline style and let a signed-in reader post. Comment bodies are server-rendered HTML
(the backend renders and sanitizes Markdown), so only the chrome is styled here.

`Kronuz.github.io`, `gmendezb-pages`, and `Xapiand/docs` carry these files and
`src/components/Discussions.astro` byte-for-byte. They are consumed two ways:

1. **Standalone** (`demo.html`) for isolated UI testing.
2. **Astro component** (`src/components/Discussions.astro`) imports `discussions.css` and
   `discussions.js` from here, so there's no copy to keep in sync.

## Files

- `discussions.css` — the timeline styles (avatars, speech bubbles, markdown body, reaction
  pills, replies, composer, sign-in button). Theme-aware: follows an ancestor
  `[data-theme="dark"]` (Starlight) and falls back to `prefers-color-scheme`.
- `discussions.js` — an IIFE that auto-mounts every `.gc` element: fetches the backend,
  renders the thread, and wires the composer and tenant OAuth sign-in.
- `demo.html` — standalone demo page (dark toggle + page-key field).

## Configuration (data attributes)

The widget mounts onto an element with class `gc`. Identify the page with a
stable `data-term` (the page slug); the backend stores this page's discussion under it:

```html
<div class="gc"
     data-term="blog/my-post"
     data-title="My Post"
     data-url="https://example.com/blog/my-post/"
     data-backend="https://comments.example/my-blog"
     data-access-key=""
     data-strip-suffix="_Acme"
     data-theme="dark"></div>
```

| Attribute | Required | Meaning |
|---|---|---|
| `data-term` | yes | Stable per-page key (the post slug). The backend stores this page's discussion under it. |
| `data-title` | no | Post title, stored as page metadata with the discussion. |
| `data-url` | no | Canonical page URL, stored as page metadata with the discussion. |
| `data-backend` | no | Tenant base URL, including its tenant path. |
| `data-access-key` | no | Static capability required by a protected tenant. Empty means public. |
| `data-strip-suffix` | no | Suffix dropped from displayed handles (e.g. `_Acme`); the full login stays the identity key. |
| `data-giphy-key` | no | Public GIPHY key. When set, the composer shows a client-side GIF picker (the browser calls GIPHY directly; rating forced to `g`). Blank = no GIF button. |
| `data-theme` | no | `light` / `dark` to force a theme; omit to follow the site. |

## Use in Astro

`src/components/Discussions.astro` wraps this widget. Drop it into any page or post:

```mdx
import Comments from '../../components/Discussions.astro';

<Comments
  backend="https://comments.example/my-blog"
  term="blog/my-post"
  title="My Post"
  url="https://example.com/blog/my-post/"
/>
```

The site decides where its backend URL lives, then passes the tenant base URL through the
`backend` prop. A protected site also passes its private build-time value through
`accessKey`. The shared wrapper accepts `PUBLIC_DISCUSSIONS_BACKEND` and
`PUBLIC_DISCUSSIONS_ACCESS_KEY` as defaults, which is useful for local development:

```bash
PUBLIC_DISCUSSIONS_BACKEND="http://127.0.0.1:8787/my-blog" npm run dev
```

The access key is embedded in the static page and is therefore visible to every reader who
can load that page. It protects a private site's comments from public discovery and direct
API access; it is not per-reader authorization.

When a site mounts comments automatically, it should use a stable page slug for `term`.
Two sites may use the same term safely because the backend scopes every discussion to its
tenant URL.

## Theming

The widget uses CSS custom properties (`--gc-bg`, `--gc-fg`, `--gc-border`, `--gc-link`,
`--gc-accent`, and others) at the top of `discussions.css`. To match the Kronuz theme, override those
variables with the site's tokens instead of editing rules. Keep `discussions.css` as the one
source. Both the Astro component and the standalone demo read it.
