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
