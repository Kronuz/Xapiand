/**
 * Build-time D2 rendering, shared by the remark plugin (```d2 fences).
 * Shells out to the d2 CLI (must be on PATH at build).
 *
 * Goal: keep D2's per-shape fill VARIATION (a mix of darker "old" cards and
 * lighter green accent cards, exactly what the brand-green ramp produces) while
 * guaranteeing readable text on every card.
 *
 *  1. Render with the brand-green `theme-overrides` ramp (varied green fills,
 *     green borders/arrows, no blue/purple) + a transparent background.
 *  2. For each node, set its label's text color to CONTRAST with that node's
 *     own fill: dark text on a light card, light text on a dark card. (D2 uses
 *     one neutral for all node text, so a single color can't work once fills are
 *     mixed; we fix it per node here.) Edge labels float on the page, so they're
 *     light on the dark theme, dark on the light theme.
 */
import { execFileSync } from "node:child_process";

// The brand-green ramp, per theme — muted (desaturated) so the diagrams read
// softer and less green. Light ramp runs dark->light; dark is its reverse.
// These are the actual fill colors D2 paints, so we also use them to decide
// whether a shape is light (needs dark text) or dark.
const RAMP = {
  light: {
    B1: "#243117", B2: "#405c2d", B3: "#5c773f", B4: "#779353", B5: "#97b073", B6: "#c6d7ae",
    AA2: "#5c773f", AA4: "#405c2d", AA5: "#c6d7ae", AB4: "#5c773f", AB5: "#e2e9d6",
  },
  dark: {
    B1: "#c6d7ae", B2: "#97b073", B3: "#779353", B4: "#5c773f", B5: "#405c2d", B6: "#243117",
    AA2: "#779353", AA4: "#97b073", AA5: "#243117", AB4: "#779353", AB5: "#1c280f",
  },
};

const TEXT = { darkOn: "#1c280f", lightOn: "#e2e9d6" };
const EDGE_LABEL = { light: "#1c280f", dark: "#e2e9d6" };
const THEME_ID = { light: 0, dark: 200 };

// Every green our ramp + text colors can legitimately produce. Anything else in
// a rendered SVG is a leaked base-theme neutral (D2's ramp doesn't remap the
// N-series), so we map only those to a green by luminance, leaving the ramp's
// varied green fills untouched.
const GREEN_RAMP = ["#243117", "#405c2d", "#5c773f", "#779353", "#97b073", "#c6d7ae", "#e2e9d6"];
const KNOWN_GREEN = new Set(
  [...Object.values({ ...RAMP.light, ...RAMP.dark }), ...GREEN_RAMP, "#1c280f"].map((h) => h.toLowerCase()),
);

const D2_CANDIDATES = ["d2", "/opt/homebrew/bin/d2", "/usr/local/bin/d2"];

function rampVars(theme) {
  const r = RAMP[theme];
  const body = Object.entries(r).map(([k, v]) => `  ${k}: "${v}"`).join("\n");
  return `vars: { d2-config: { theme-overrides: {\n${body}\n} } }\nstyle.fill: transparent\n`;
}

function luminance(hex) {
  const r = parseInt(hex.slice(1, 3), 16);
  const g = parseInt(hex.slice(3, 5), 16);
  const b = parseInt(hex.slice(5, 7), 16);
  return (0.299 * r + 0.587 * g + 0.114 * b) / 255;
}

// Map a leaked (non-green) base color to a brand green of similar luminance.
function toGreen(hex) {
  return GREEN_RAMP[Math.min(GREEN_RAMP.length - 1, Math.floor(luminance(hex) * GREEN_RAMP.length))];
}

function runD2(input, themeId, pad) {
  let notFound;
  for (const bin of D2_CANDIDATES) {
    try {
      return execFileSync(
        bin,
        ["--sketch", "--pad", String(pad), "--theme", String(themeId), "-", "-"],
        // Capture stderr instead of letting it inherit to the console: d2 prints a
        // "success: successfully compiled ..." line to stderr on every render, which
        // would otherwise spam the build/dev output (~one per diagram). Piping it
        // keeps it quiet and still exposes it as err.stderr for the error path below.
        { input, encoding: "utf8", maxBuffer: 16 * 1024 * 1024, stdio: ["pipe", "pipe", "pipe"] },
      );
    } catch (err) {
      if (err && err.code === "ENOENT") {
        notFound = err;
        continue;
      }
      const detail = err && err.stderr ? String(err.stderr).trim() : String(err);
      throw new Error(`D2: failed to render the diagram:\n${detail}`);
    }
  }
  throw new Error(
    `D2: could not find the d2 CLI on PATH (install it and build with d2 available). ${String(notFound)}`,
  );
}

/** Render one theme of a D2 diagram to a data-URI SVG string. */
export function renderD2(code, theme, pad = 24) {
  let svg = runD2(rampVars(theme) + code, THEME_ID[theme], pad);
  svg = svg.slice(svg.indexOf("<svg")); // drop the XML prolog

  // Walk shapes and node labels in document order; each node's label text
  // follows its shape, so track the last shape fill and contrast the text to it.
  const ramp = RAMP[theme];
  let lastFill = "#24390f";
  svg = svg.replace(
    /class="shape[^"]*\bfill-(B[1-6]|AA[2-5]|AB[2-5])\b[^"]*"|<text\b[^>]*\bfill-N1\b[^>]*>/g,
    (m, fillClass) => {
      if (fillClass) {
        lastFill = ramp[fillClass] || lastFill;
        return m;
      }
      const color = luminance(lastFill) > 0.5 ? TEXT.darkOn : TEXT.lightOn;
      // Set via inline style (not the fill= attribute): D2 embeds a
      // `.fill-N1 { fill: ... }` class rule, and CSS beats presentation
      // attributes, so an inline style with !important is what actually wins.
      return m.includes('style="')
        ? m.replace('style="', `style="fill:${color} !important;`)
        : m.replace(/<text\b/, `<text style="fill:${color} !important"`);
    },
  );

  // Edge labels float on the page, not on a card: theme-contrasted via CSS.
  const inject = `<style>[class*="fill-N2"]{fill:${EDGE_LABEL[theme]} !important}</style>`;
  svg = svg.replace("</svg>", inject + "</svg>");

  // De-leak: map any remaining non-green hex (leaked base-theme neutrals) to a
  // green; the ramp's varied green fills are in KNOWN_GREEN, so they're kept.
  svg = svg.replace(/#[0-9A-Fa-f]{6}/g, (h) =>
    KNOWN_GREEN.has(h.toLowerCase()) ? h : toGreen(h),
  );
  return `data:image/svg+xml,${encodeURIComponent(svg)}`;
}

function escapeAttr(str) {
  return String(str)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

/** Render both themes into the `figure.diagram` HTML (light + dark images). */
export function figureHtml(code, alt = "", pad = 24) {
  const light = renderD2(code, "light", pad);
  const dark = renderD2(code, "dark", pad);
  const a = escapeAttr(alt);
  return (
    `<figure class="diagram">` +
    `<img class="diagram-light" src="${light}" alt="${a}" />` +
    `<img class="diagram-dark" src="${dark}" alt="${a}" />` +
    `</figure>`
  );
}
