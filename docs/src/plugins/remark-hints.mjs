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

// Custom "hint" asides for the six Jekyll hint styles (tip / info / caution / warning /
// unimplemented / construction) -- each gets its own colour and icon, which Starlight's
// four built-in aside types (note/tip/caution/danger) can't express. The converter emits
// `:::hint[Title]{.type}\nbody\n:::`; remark-directive (already in Starlight's pipeline)
// parses that into a containerDirective, and this plugin turns it into
// `<aside class="kz-hint kz-hint-<type>">` with a `<p class="kz-hint-title">`. The body
// keeps rendering as markdown (it stays mdast children). Styling lives in custom.css.
import { visit } from 'unist-util-visit';

const TYPES = new Set(['tip', 'info', 'caution', 'warning', 'unimplemented', 'construction', 'note']);

export default function remarkHints() {
	return (tree) => {
		visit(tree, 'containerDirective', (node) => {
			if (node.name !== 'hint') return;
			let type = (node.attributes && node.attributes.class) || 'note';
			if (!TYPES.has(type)) type = 'note';

			const data = node.data || (node.data = {});
			data.hName = 'aside';
			data.hProperties = { className: ['kz-hint', `kz-hint-${type}`] };

			// The [Title] label is the first child paragraph, flagged directiveLabel;
			// render it as the aside's title line.
			const label = node.children.find((c) => c.data && c.data.directiveLabel);
			if (label) {
				const ld = label.data || (label.data = {});
				ld.hName = 'p';
				ld.hProperties = { className: ['kz-hint-title'] };
			}
		});
	};
}
