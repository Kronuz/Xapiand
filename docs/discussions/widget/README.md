# Discussions widget

The front-end half of the self-hosted discussions system: a small, framework-free
`discussions.css` + `discussions.js` that render a page's comments in the GitHub/giscus
timeline style and let a signed-in reader post. Comment bodies are server-rendered HTML
(the backend renders Markdown locally with cmark-gfm, GitHub's own renderer), so they
match GitHub exactly; only the chrome is styled here.

This is the single source of truth for the widget. It's consumed two ways:

1. **Standalone** (`demo.html`) — served by the backend at `/demo` for local testing.
2. **Astro component** (`src/components/Discussions.astro`) — imports `discussions.css` and
   `discussions.js` from here, so there's no copy to keep in sync.

## Files

- `discussions.css` — the timeline styles (avatars, speech bubbles, markdown body, reaction
  pills, replies, composer, sign-in button). Theme-aware: follows an ancestor
  `[data-theme="dark"]` (Starlight) and falls back to `prefers-color-scheme`.
- `discussions.js` — an IIFE that auto-mounts every `.gc` element: fetches the backend,
  renders the thread, wires the composer and the GitHub sign-in popup.
- `demo.html` — standalone demo page (dark toggle + page-key field).

## Configuration (data attributes)

The widget mounts onto an element with class `gc`. Identify the page with a
stable `data-term` (the page slug); the backend stores this page's discussion under it:

```html
<div class="gc"
     data-term="blog/my-post"
     data-title="My Post"
     data-url="https://example.com/blog/my-post/"
     data-backend="https://your-backend.example:8443"
     data-strip-suffix="_Acme"
     data-theme="dark"></div>
```

| Attribute | Required | Meaning |
|---|---|---|
| `data-term` | yes | Stable per-page key (the post slug). The backend stores this page's discussion under it. |
| `data-title` | no | Post title, stored as page metadata with the discussion. |
| `data-url` | no | Canonical page URL, stored as page metadata with the discussion. |
| `data-backend` | no | Backend base URL. Empty = same origin (only useful on the backend's own `/demo`). |
| `data-strip-suffix` | no | Suffix dropped from displayed handles (e.g. `_Acme`); the full login stays the identity key. |
| `data-giphy-key` | no | Public GIPHY key. When set, the composer shows a client-side GIF picker (the browser calls GIPHY directly; rating forced to `g`). Blank = no GIF button. |
| `data-theme` | no | `light` / `dark` to force a theme; omit to follow the site. |

## Use in Astro

`src/components/Discussions.astro` wraps this widget. Drop it into any page or post:

```mdx
import Comments from '../../components/Discussions.astro';

<Comments term="blog/my-post" title="My Post" url="https://example.com/blog/my-post/" />
```

The backend URL comes from `site.config.json` (`comments.backendUrl`), wired through
`consts.ts` → `MarkdownContent.astro`, which passes it to `<Comments>` as the `backend` prop —
so a normal `npm run build` / `npm run deploy` bakes it in. For local development you can
override it with the `PUBLIC_DISCUSSIONS_BACKEND` env var (e.g. to point at a backend on
`127.0.0.1`), or pass `backend="..."` per instance:

```bash
PUBLIC_DISCUSSIONS_BACKEND="http://127.0.0.1:8099" npm run dev
```

`MarkdownContent.astro` already renders `<Comments>` on every blog post, keyed by the
post slug (`entry.id`) — so new posts get comments with no per-post setup. `discussion:`
in frontmatter is just an optional explicit key override.

## Theming

The widget uses CSS custom properties (`--gc-bg`, `--gc-fg`, `--gc-border`, `--gc-link`,
`--gc-accent`, …) at the top of `discussions.css`. To match the Kronuz theme, override those
variables with the site's tokens instead of editing rules. Keep `discussions.css` as the one
source — both the Astro component and the standalone demo read it.
