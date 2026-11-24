# Docs site: reading and navigation

The docs controls come from `Kronuz.github.io:297524c` and
`Kronuz.github.io:37728df`. The pinned `@kronuz/starlight-core` package supplies
shared components, Markdown plugins, themes, docs CSS, and footer to all four
sites. Xapiand owns its content, navigation, logo, footer identity, and Astro
configuration. The reading CSS uses
Expressive Code's `--ec-codeFontSize`, and the edit link serves any docs
entry. The local-editor URL inserts a slash after Astro's `/Xapiand`
base path. The existing sidebar already gives each page one filled link, so the
blog's ancestor hint is unnecessary here. The Xapiand editor base URL
uses the tracked `site/` directory.

- **One filled sidebar link.** The existing sidebar gives `aria-current="page"`
  only to the current page; its custom sublist needs no ancestor conversion.
- **Reading size.** The shared theme selector puts Aa before the theme selector on
  desktop; `Search.astro` puts it after search on phones. Exactly one is
  visible on a docs page, and splash pages hide both. The toggle stores
  `kz-text-size`; route middleware restores it from `<head>` before paint.
  Inside `<main>`, Starlight's type tokens grow by 1.125; the content-width
  token grows from 45rem to 48.6rem at the root. Lato needs this
  narrower scale to keep lines near 98 characters. The header and sidebars
  keep their size. The 300px TOC minimum caps the panel at 761px at a
  1440px viewport; the toggle preserves the reading point.
- **Image zoom.** The shared Markdown content wrapper mounts the desktop-only modal. A
  content image is zoomable only if the fitted modal enlarges it by at least
  1.15x. Linked images, snippets, code, and touch input are left alone.
- **Unified footer.** The package renders copyright, edit metadata, pagination,
  and "Proudly hosted by GitHub" in one centered footer.
- **Author edit links.** The shared edit link renders "Edit source" in the footer,
  with a local-editor icon in development. In production, `?editor=on` opts
  in and `?editor=off` opts out; the `kz-editor` choice persists across pages.
  The title row holds only the title.
- **Code copy on touch screens.** The shared Markdown content wrapper mounts
  `CopyReveal.astro` (`Kronuz.github.io:480b250`). Tapping a code block
  reveals its Expressive Code Copy button; tapping the block again hides it.
  The CSS keeps Copy hidden until
  that tap or keyboard focus. Desktop hover retains Expressive Code's behavior.
