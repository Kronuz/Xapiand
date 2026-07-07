// Expressive Code plugin: make the copy-to-clipboard button on ```rest blocks copy
// the equivalent `curl` command instead of the raw REST snippet. The displayed block
// is unchanged (still the readable "METHOD /url + headers + body" form); only the
// copied text becomes a runnable curl. This restores the behaviour the old (v0.4.0)
// docs advertised on welcome.md ("copy the equivalent curl code ...").
//
// It runs in postprocessRenderedBlock, after plugin-frames has already built the copy
// button, and simply rewrites that button's `data-code` (newlines encoded as U+007F,
// the same encoding the frames copy script decodes back to real newlines).

const HOST = "http://localhost:8880";

/** Turn a ```rest snippet body into a curl command string (real newlines). */
export function restToCurl(code) {
	const lines = code.split("\n");

	let i = 0;
	while (i < lines.length && lines[i].trim() === "") i++;
	if (i >= lines.length) return null;

	const reqMatch = /^([A-Za-z]+)\s+(\S+)(?:\s+HTTP\/\S+)?\s*$/.exec(lines[i].trim());
	if (!reqMatch) return null;
	const method = reqMatch[1].toUpperCase();
	const url = reqMatch[2];
	i++;

	const headers = [];
	for (; i < lines.length; i++) {
		if (lines[i].trim() === "") {
			i++;
			break;
		}
		const hMatch = /^([A-Za-z0-9-]+):\s*(.*)$/.exec(lines[i]);
		if (!hMatch) break;
		headers.push([hMatch[1], hMatch[2].trim()]);
	}

	const body = lines.slice(i).join("\n").trim();
	const hasContentType = headers.some(([k]) => k.toLowerCase() === "content-type");

	const parts = [`curl -X ${method} '${HOST}${url}'`];
	for (const [k, v] of headers) parts.push(`-H '${k}: ${v}'`);
	if (body && !hasContentType) parts.push(`-H 'Content-Type: application/json'`);
	if (body) parts.push(`-d '${body.replace(/'/g, `'\\''`)}'`);

	return parts.join(" \\\n  ");
}

export function pluginRestCurl() {
	return {
		name: "rest-curl",
		hooks: {
			postprocessRenderedBlock: ({ codeBlock, renderData }) => {
				if (codeBlock.language !== "rest") return;
				const curl = restToCurl(codeBlock.code);
				if (!curl) return;
				const encoded = curl.replace(/\n/g, "\x7F");
				const walk = (node) => {
					if (!node || typeof node !== "object") return;
					if (node.type === "element" && node.properties && node.properties.dataCode != null) {
						node.properties.dataCode = encoded;
					}
					if (Array.isArray(node.children)) node.children.forEach(walk);
				};
				walk(renderData.blockAst);
			},
		},
	};
}
