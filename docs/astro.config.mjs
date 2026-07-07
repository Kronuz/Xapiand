// @ts-check
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

import kronuzDark from './src/styles/kronuz-dark.json';
import kronuzLight from './src/styles/kronuz-light.json';
import { sidebar } from './src/sidebar.mjs';
import { restLangs } from './src/rest-lang.mjs';
import remarkHints from './src/remark-hints.mjs';
import remarkD2 from './src/remark-d2.mjs';
import { pluginRestCurl } from './src/ec-rest-curl.mjs';

// Xapiand documentation, on Astro + Starlight (Kronuz theme), a migration of the
// former Jekyll site. Search is Starlight's built-in Pagefind (local, static, no
// API key) -- replacing the dead Algolia DocSearch that pointed at kronuz.io.
export default defineConfig({
  site: 'https://kronuz.github.io',
  base: '/Xapiand',
  // The six Jekyll hint styles (tip/info/caution/warning/unimplemented/construction) are
  // rendered as custom `:::hint{.type}` asides; see src/remark-hints.mjs + custom.css.
  markdown: {
    // remarkD2 turns ```d2 fences into themed (light+dark) diagrams at build time
    // by shelling out to the d2 CLI (must be on PATH); see src/remark-d2.mjs.
    remarkPlugins: [remarkHints, remarkD2],
  },
  // The Jekyll site served a /tutorials/ index; keep that URL working
  // (external/bookmarked links) by redirecting to its Astro equivalent.
  redirects: {
    '/tutorials': '/Xapiand/tutorials/home',
  },
  integrations: [
    starlight({
      title: 'Xapiand',
      description: 'Search and Storage Server',
      logo: { src: './src/assets/logo.png', alt: 'Xapiand' },
      favicon: '/favicon.ico',
      customCss: ['./src/styles/custom.css'],
      // The landing page uses the Jekyll site's typefaces (Lato body, Montserrat
      // display) so it reads like the old home; load them the same way Jekyll did.
      head: [
        { tag: 'link', attrs: { rel: 'preconnect', href: 'https://fonts.gstatic.com', crossorigin: true } },
        {
          tag: 'link',
          attrs: {
            rel: 'stylesheet',
            href: 'https://fonts.googleapis.com/css2?family=Lato:ital,wght@0,300;0,400;0,700;1,300;1,400;1,700&family=Montserrat:wght@800&display=swap',
          },
        },
      ],
      // "Edit this page" -> GitHub, like the Jekyll "Improve this page" link.
      editLink: {
        baseUrl: 'https://github.com/Kronuz/Xapiand/edit/master/docs-astro/',
      },
      expressiveCode: { themes: [kronuzDark, kronuzLight], shiki: { langs: restLangs }, plugins: [pluginRestCurl()] },
      social: [
        { icon: 'github', label: 'GitHub', href: 'https://github.com/Kronuz/Xapiand' },
      ],
      // The full curated nav, transformed 1:1 from the Jekyll _data/docs.yaml by
      // scripts/gen-sidebar.mjs. Top groups start expanded (collapsed: false).
      sidebar,
      components: {
        // Site title/logo, plus the top-level Docs / Help / About nav (Jekyll header).
        SiteTitle: './src/components/SiteTitle.astro',
        // Edit link moves onto the page-title line (right-aligned); the stock footer
        // edit link renders nothing.
        PageTitle: './src/components/PageTitle.astro',
        EditLink: './src/components/EditLink.astro',
        // Server-rendered sidebar: clickable group headers, chevron-only toggle, groups
        // stay expanded when they contain the current page.
        Sidebar: './src/components/Sidebar.astro',
        // "On this page" leads with the page name, not a generic "Overview".
        TableOfContents: './src/components/TableOfContents.astro',
        // Same fix for the mobile "On this page" dropdown.
        MobileTableOfContents: './src/components/MobileTableOfContents.astro',
        // Prev/next pager shows each destination's real title, not its "Overview" label.
        Pagination: './src/components/Pagination.astro',
      },
    }),
  ],
});
