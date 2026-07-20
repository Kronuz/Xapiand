/** Normalize terminal-valid extended SGR colors before Shiki renders ```ansi. */
import { visit } from 'unist-util-visit';
import { normalizeAnsiForShiki } from '../lib/normalizeAnsi.mjs';

export default function remarkAnsi() {
	return (tree) => {
		visit(tree, 'code', (node) => {
			if (node.lang?.toLowerCase() === 'ansi') {
				node.value = normalizeAnsiForShiki(node.value);
			}
		});
	};
}
