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
 * Convert valid colon-subparameter SGR colors into the semicolon form Shiki's
 * special `ansi` language understands. Real terminals accept both forms.
 *
 * Only complete CSI ... m sequences are touched. Malformed captures remain
 * unchanged so an ANSI audit can still find them instead of silently hiding them.
 */
const SGR = /\x1b\[([0-9:;]*)m/g;

function byte(value) {
	return /^\d+$/.test(value) && Number(value) <= 255;
}

function normalizeColor(parameter) {
	const parts = parameter.split(':');
	const [channel, mode] = parts;
	if (channel !== '38' && channel !== '48') return parameter;

	if (mode === '5' && parts.length === 3 && byte(parts[2])) {
		return `${channel};5;${parts[2]}`;
	}

	if (mode === '2') {
		// Terminals emit both 38:2:R:G:B and the standards-oriented
		// 38:2:COLORSPACE:R:G:B form (COLORSPACE is often empty or 1).
		const rgb = parts.length === 5 ? parts.slice(2, 5) : parts.slice(3, 6);
		if ((parts.length === 5 || parts.length >= 6) && rgb.every(byte)) {
			return `${channel};2;${rgb.join(';')}`;
		}
	}

	return parameter;
}

export function normalizeAnsiForShiki(source) {
	return source.replace(SGR, (sequence, parameters) => {
		const normalized = parameters.split(';').map(normalizeColor).join(';');
		return `\x1b[${normalized}m`;
	});
}
