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

// Expressive Code plugin: make the copy-to-clipboard button on ```rest blocks copy
// the equivalent `curl` command instead of the raw REST snippet. The displayed block
// is unchanged (still the readable "METHOD /url + headers + body" form); only the
// copied text becomes a runnable curl. This restores the behaviour the old (v0.4.0)
// docs advertised on welcome.md ("copy the equivalent curl code ...").
//
// It runs in postprocessRenderedBlock, after plugin-frames has already built the copy
// button, and simply rewrites that button's `data-code` (newlines encoded as U+007F,
// the same encoding the frames copy script decodes back to real newlines).

const DEFAULT_HOST = "http://localhost:8880";

/** Turn a ```rest snippet body into a curl command string (real newlines). */
export function restToCurl(code, host = DEFAULT_HOST) {
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

	const parts = [`curl -X ${method} '${host}${url}'`];
	for (const [k, v] of headers) parts.push(`-H '${k}: ${v}'`);
	if (body && !hasContentType) parts.push(`-H 'Content-Type: application/json'`);
	if (body) parts.push(`-d '${body.replace(/'/g, `'\\''`)}'`);

	return parts.join(" \\\n  ");
}

// Per-site: pass `{ host }` to set the base URL used for the copied curl (defaults
// to Xapiand's http://localhost:8880). Blogs that carry the goody but target a
// different service can override it, e.g. pluginRestCurl({ host: 'https://api.example' }).
export function pluginRestCurl({ host = DEFAULT_HOST } = {}) {
	return {
		name: "rest-curl",
		hooks: {
			postprocessRenderedBlock: ({ codeBlock, renderData }) => {
				if (codeBlock.language !== "rest") return;
				const curl = restToCurl(codeBlock.code, host);
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
