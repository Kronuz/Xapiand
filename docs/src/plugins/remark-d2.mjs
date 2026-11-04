// Copyright © 2026 Germán Méndez Bravo (Kronuz)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

/**
 * remark-d2 — turn fenced ```d2 code blocks into themed diagrams at build time.
 *
 *     ```d2 alt="what the diagram shows"
 *     direction: down
 *     a -> b
 *     ```
 *
 * The fence body is raw text (no escaping, no String.raw), so D2's `\n` label
 * breaks and `{ }` shapes are passed through verbatim. An optional alt="..." in
 * the info string becomes the image alt text. Rendering is shared with the
 * <D2Diagram> component via ../lib/renderD2.mjs.
 */
import { visit } from "unist-util-visit";
import { figureHtml } from "../lib/renderD2.mjs";

export default function remarkD2() {
  return (tree) => {
    visit(tree, "code", (node, index, parent) => {
      if (!parent || node.lang !== "d2") return;
      const match = node.meta ? /alt="([^"]*)"/.exec(node.meta) : null;
      const alt = match ? match[1] : "";
      parent.children[index] = { type: "html", value: figureHtml(node.value, alt) };
    });
  };
}
