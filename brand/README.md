# Brand

`logo.svg` is the vector Xapiand logo: a green rounded hexagon with three stacked white layers,
inside a white rim. It redraws the 180×230 raster `docs/src/assets/logo.png` as exact geometry,
measured from that raster's pixels, in three paths: `rim`, `hexagon` and `layers`. Its origin is
the hexagon's center, in the raster's pixel units.

| Part | Geometry |
|---|---|
| Rim | Regular hexagon, half-width 90, corners rounded with radius 39.3, then stretched 1.039 vertically (so its corners are ellipse arcs) |
| Hexagon | The rim scaled by 5/6 about the center: half-width 75, corner radius 32.75 |
| Layer | A square rotated 45° (half-diagonal 52.66, corner radius 6.11), then scaled 0.5848 vertically |
| Stack | Three layers centered at −26.66, 0 and +26.66; each lower one is cut by the layer above moved down 10.58 |

The raster drew its layers 0.73 px right of the hexagon's axis and its middle layer 0.61 px low;
the master centers the stack and spaces the layers evenly. `logo-vs-png.png` shows the raster,
the master at the same size and larger, and their difference.

`node brand/build-icons.mjs`, run from the repository root after `npm ci` in `docs/`, renders
every variant from the master into `variants/<name>/`, rasterizing each PNG once at its exact size:

- `logo.svg` is the master in the variant's colors.
- `favicon.svg` serves tabs at any DPR, and `favicon.ico` (16, 32 and 48 px frames) is the
  fallback. At those three sizes a gamma on alpha firms the outline.
- `apple-touch-icon.png` (180 px), `-167` and `-152` serve iOS, and `icon-192.png` and
  `icon-512.png` serve Android. They are opaque tiles in the variant's page color, because iOS and
  Android flatten a transparent icon onto white. The tile takes the rim's place, and the hexagon
  fills 74% of its height, inside the maskable safe circle (r = 40%).

| Variant | Colors | For |
|---|---|---|
| `light` | The master: green `#8fc44a`, white layers and rim; tiles `#fdfdfc` | Light pages; the live variant |
| `dark` | No rim, so no white halo on a dark page; tiles `#1a1a1a` | Dark pages |

`light` is the current logo and the one meant to be live. The site still serves
`docs/src/assets/logo.png` and `docs/public/favicon.ico`; moving it to `light` (and to `dark` for
the dark theme's page logo) is a separate step.
