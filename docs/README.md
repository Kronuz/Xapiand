# Xapiand docs site

This directory contains the code for the [Xapiand docs site](https://Kronuz.github.io/Xapiand).


## Contributing

For information about contributing, see the [Contributing page](https://Kronuz.github.io/Xapiand/contributing/).


## Running locally

The site is built with [Jekyll](https://jekyllrb.com/) through the `github-pages`
gem — the same stack GitHub Pages uses to build it server-side (currently Jekyll
3.10).

**1. Use a modern Ruby.** macOS ships Ruby 2.6, which is too old for current
Bundler (it needs Ruby ≥ 3.2, and the old pinned toolchain won't build on Apple
silicon). Install a supported Ruby (3.1–3.3) and put it on your `PATH` — e.g. with
Homebrew:

```sh
brew install ruby@3.3
export PATH="/opt/homebrew/opt/ruby@3.3/bin:$PATH"   # add to ~/.zshrc to persist
```

**2. Install the gems** (from this `docs/` directory):

```sh
gem install bundler
bundle config set --local path vendor/bundle   # keeps gems in docs/vendor (gitignored)
bundle install
```

**3. Serve:**

```sh
bundle exec jekyll serve --livereload --incremental
```

Open **<http://localhost:4000/Xapiand/>**. The `/Xapiand/` prefix comes from
`baseurl` in `_config.yaml`, so the bare `http://localhost:4000/` returns 404 —
that's expected. Pages reload automatically as you edit.


## Updating Font Awesome
Only a handful of fonts are included in the include Font Awesome fonts. To add
more, it's needed to modify `icomoon-selection.json` and regenerate the fonts:

1. Go to <https://icomoon.io/app/>
2. Choose Import Icons and load `icomoon-selection.json`
3. Choose Generate Font → Download
4. Copy the font files and adapt the CSS to the paths we use
