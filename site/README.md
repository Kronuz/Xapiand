# Xapiand documentation site

This directory builds the [Xapiand documentation](https://kronuz.github.io/Xapiand/)
with Astro and Starlight. The shared reading controls, Markdown renderers, themes,
and footer come from the pinned [`@kronuz/starlight-core`](https://github.com/Kronuz/starlight-core)
package. Design notes and campaign records live in [`../docs/`](../docs/).

## Run locally

Install Node.js 22.12 or newer and the [D2 CLI](https://d2lang.com/) for diagrams.
From this directory:

```sh
npm ci
npm run dev
npm run build
```

The build writes static pages and a Pagefind search index to `dist/`. The site
uses the `/Xapiand/` base path in development and on GitHub Pages.

## Source

- Pages and request examples live in `src/content/docs/`.
- `src/lib/sidebar.mjs` defines navigation and the docs E2E page order.
- `src/starlightRouteData.ts` supplies the reader preferences and site footer data.
- `public/assets/` contains examples used by both the site and test harness.

Pushing `site/` changes to `master` runs `.github/workflows/pages.yml`. The
workflow builds and deploys the static site to GitHub Pages.
