# xapiand-brand: Xapiand has a brand/ directory with a vector master logo and its icon variants

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status xapiand-brand`.

## Outcome

Owner ask (relayed by Detent's lead, 2026-11-11): Xapiand gets a `brand/` directory laid out like
Kronuz's (`~/code/KronuzBlog/Kronuz.github.io/brand/`: README.md, the vector master, and
`variants/<name>/` with favicon.svg, favicon.ico (16/32/48), apple-touch-icon.png (180),
apple-touch-icon-152.png, apple-touch-icon-167.png, icon-192.png, icon-512.png and logo.svg in that
variant's colors). Today Xapiand has only a 180x230 raster, `docs/src/assets/logo.png`: a green
rounded hexagon with three stacked white layers.

## Acceptance criteria

- A1 [artifact] `brand/logo.svg`: the current logo redrawn as exact geometry (hexagon, layers,
  their gaps and corner radii measured from `docs/src/assets/logo.png`), not an autotrace; no
  raster inside, no fonts. Rendered at 180x230 it matches the PNG to the eye beside it (the
  journal shows both renders side by side).
- A2 [artifact] `brand/variants/<name>/` for at least two variants: the current green on light
  pages, and one that reads on a dark page; each with the full Kronuz set of files.
- A3 `docs/scripts/build-icons.mjs` (`npm run icons` in docs/) generates every variant file from
  the master, following Kronuz's `scripts/build-icons.mjs` (reuse its approach: exact-size
  rasterization, tab-size alpha gamma, maskable safe area); the `icons` check regenerates and
  finds no diff.
- A4 [artifact] `brand/README.md`: what the master is, how the icons derive, which variant is meant
  to be live.

## Contracts

None.

## Decisions

Lead: the site keeps its current assets; switching docs/public and the page logo to the live variant
is a separate step the owner decides. Detent's `detent-brand` follows the same layout; match it
when it lands.

## May land early

None.
