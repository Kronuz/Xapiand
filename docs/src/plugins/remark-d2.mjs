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
