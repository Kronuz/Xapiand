/**
 * Local Markdown -> sanitized, syntax-highlighted HTML, the self-hosted store's renderer.
 *
 * The unified remark/rehype pipeline targets GitHub Flavored Markdown, so output supports
 * tables, strikethrough, task lists, autolinks, and footnotes.
 *
 * XSS safety: rehype-sanitize uses GitHub's allow-list, so raw HTML is
 * dropped and dangerous URL schemes (javascript:, data:, ...) are neutralized.
 *
 * Syntax highlighting: done here with **Shiki** and bundled light and dark themes. To keep
 * the Worker bundle small we use Shiki's fine-grained core with the
 * pure-JS regex engine (no WASM) and a curated language set. Dual-theme output emits
 * CSS-variable styles (`--shiki` / `--shiki-dark`), so one render supports light+dark and no
 * highlight.css token table is needed (the widget's highlight.css only flips the dark vars).
 * Shiki runs AFTER rehype-sanitize (it adds our own style spans to already-safe code text),
 * and the result is cached in D1, so this runs once per add/edit, not per read. HARDBREAKS
 * (a single newline -> <br>, like GitHub comment bodies) is on via remark-breaks.
 */
import rehypeShikiFromHighlighter from "@shikijs/rehype/core";
import rehypeSanitize from "rehype-sanitize";
import rehypeStringify from "rehype-stringify";
import remarkBreaks from "remark-breaks";
import remarkGfm from "remark-gfm";
import remarkParse from "remark-parse";
import remarkRehype from "remark-rehype";
import { createHighlighterCore, type HighlighterCore } from "shiki/core";
import { createJavaScriptRegexEngine } from "shiki/engine/javascript";
import type { HighlighterGeneric, ThemeRegistrationAny } from "@shikijs/types";
import type { Processor } from "unified";
import { unified } from "unified";

import bash from "@shikijs/langs/bash";
import c from "@shikijs/langs/c";
import cpp from "@shikijs/langs/cpp";
import css from "@shikijs/langs/css";
import go from "@shikijs/langs/go";
import html from "@shikijs/langs/html";
import json from "@shikijs/langs/json";
import markdown from "@shikijs/langs/markdown";
import python from "@shikijs/langs/python";
import rust from "@shikijs/langs/rust";
import typescript from "@shikijs/langs/typescript";
import javascript from "@shikijs/langs/javascript";
import yaml from "@shikijs/langs/yaml";

import discussionsDark from "./themes/discussions-dark.json";
import discussionsLight from "./themes/discussions-light.json";

// Curated language set (kept small for the free-tier bundle budget). Unlisted languages fall
// back to plain text, so an exotic fence still renders (just uncolored).
const LANGS = [javascript, typescript, python, bash, json, c, cpp, rust, go, yaml, html, css, markdown];

// The highlighter (and the processor that uses it) are built lazily and memoized per isolate:
// createHighlighterCore is async, and this cost is paid once per cold isolate, not per render.
let _processor: Promise<Processor> | null = null;

async function build(): Promise<Processor> {
  const highlighter: HighlighterCore = await createHighlighterCore({
    // Our themes are VS Code-style (colors + tokenColors), which Shiki accepts at runtime;
    // the cast bridges that to Shiki's stricter resolved-theme type.
    themes: [discussionsLight as unknown as ThemeRegistrationAny, discussionsDark as unknown as ThemeRegistrationAny],
    langs: LANGS,
    engine: createJavaScriptRegexEngine(),
  });
  return unified()
    .use(remarkParse)
    .use(remarkGfm)
    .use(remarkBreaks)
    .use(remarkRehype)
    .use(rehypeSanitize)
    .use(rehypeShikiFromHighlighter, highlighter as unknown as HighlighterGeneric<string, string>, {
      themes: { light: "discussions-light", dark: "discussions-dark" },
      defaultLanguage: "text",
      fallbackLanguage: "text",
    })
    .use(rehypeStringify) as unknown as Processor;
}

function getProcessor(): Promise<Processor> {
  if (!_processor) _processor = build();
  return _processor;
}

/** Render Markdown to sanitized, GitHub-compatible HTML with Shiki-highlighted code. */
export async function render(text: string): Promise<string> {
  const processor = await getProcessor();
  const file = await processor.process(text || "");
  return String(file);
}
