# Xapiand docs on Astro + Starlight (migration prototype)

A prototype migration of the Jekyll `../docs/` site to **Astro + Starlight**, reusing
the Kronuz theme from `KronuzBlog/Kronuz.github.io`. It is parallel to and independent
of the existing Jekyll site (nothing in `../docs/` is touched), so it is fully
reversible.

## Why

- **Search is broken** in the Jekyll site: Algolia DocSearch pointed at the dead
  `kronuz.io` domain, and the account is no longer reachable. Starlight ships
  **Pagefind** (local, static, build-time search, no API key, no external service),
  which replaces it for free.
- Match the Starlight look/feel of the blog, keep all content, keep the full curated
  sidebar, and keep the embedded E2E tests.

## What's proven here

- **Builds:** `npm run build` → 196 pages, and a **Pagefind index** under `dist/pagefind/`.
- **Full sidebar**, transformed 1:1 from `../docs/_data/docs.yaml` by
  `scripts/gen-sidebar.mjs` (the four groups Getting Started / Reference Guide / Set up
  / Miscellaneous, deep nesting, top groups expanded).
- **Content:** `scripts/convert-docs.mjs` converts `../docs/_docs` + `../docs/_tutorials`
  (108 pages), plus a splash home (`src/content/docs/index.mdx`).
- **Embedded E2E tests survive:** a request's ` ```json ` block renders (Expressive Code);
  the `pm.test` / `status:` / `params:` payload is wrapped in an `<!-- e2e:begin … e2e:end -->`
  HTML comment (hidden from render, preserved in source). `docs_to_postman.py` still
  extracts requests + assertions from the migrated `.md` unchanged.

## Commands

```sh
npm install
npm run sidebar   # regenerate src/sidebar.mjs from ../docs/_data/docs.yaml
npm run convert   # regenerate src/content/docs from ../docs/_docs (+ tutorials)
npm run dev       # preview at http://localhost:4321/Xapiand
npm run build     # build to dist/ (also builds the Pagefind index)
```

## Known prototype gaps (not blockers)

- The converters are best-effort: a few Jekyll-isms (some `{: .note }` asides, edge
  Liquid) may need cleanup; `../docs/tests/` (test-only docs) is not migrated yet.
- The request example renders as a plain ` ```json ` block; the real migration should add
  a small remark plugin to render it as the curl card (copy-as-curl button), replacing
  `_includes/curl.html`.
- Two sidebar slugs (`reference-guide` overview, `tutorials/home`) are stubs.
- Single-version for now; multi-version (v0.4.0 / v1.0.0) is a later phase
  (`starlight-versions` plugin or per-version dirs).
