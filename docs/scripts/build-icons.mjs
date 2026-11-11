#!/usr/bin/env node
// Renders every icon variant from the vector master, brand/logo.svg (`npm run icons`), into
// brand/variants/<name>/. Each PNG is rasterized once, at its exact pixel size.
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import sharp from 'sharp';

// Fills of the master's three paths, and the opaque tile behind the touch icons. iOS and
// Android flatten a transparent icon onto white, so the tile is the page color of the
// variant's theme. A variant without a rim draws only the hexagon and its layers.
const VARIANTS = {
	light: { rim: '#fff', hexagon: '#8fc44a', layers: '#fff', tile: '#fdfdfc' }, // the master, for light pages
	dark: { rim: null, hexagon: '#8fc44a', layers: '#fff', tile: '#1a1a1a' }, // no white rim, for dark pages
};
const INNER = 5 / 6; // the hexagon is the rim scaled by 5/6 about the center (brand/README.md)
// Stem darkening for tab sizes: the outline only partly covers its pixels there, and a gamma
// on alpha firms it up.
const GAMMA = { 16: 0.7, 32: 0.7, 48: 0.85 };

const root = fileURLToPath(new URL('../../', import.meta.url));
const master = readFileSync(`${root}brand/logo.svg`, 'utf8');
const [, viewBox, body] = master.match(/viewBox="([^"]+)"[^>]*>([\s\S]*)<\/svg>/);
const [x, y, w, h] = viewBox.trim().split(/\s+/).map(Number);
const cx = x + w / 2;
const cy = y + h / 2;
const n = (v) => +v.toFixed(2);
const box = (bw, bh) => `${n(cx - bw / 2)} ${n(cy - bh / 2)} ${n(bw)} ${n(bh)}`;

// The master's paths in the variant's fills, without the rim when `rim` is null.
function paint(colors, rim = colors.rim) {
	const art = body.replace(/<path id="(\w+)" fill="[^"]*"/g, (_, id) => `<path id="${id}" fill="${colors[id]}"`);
	return rim ? art : art.replace(/<path id="rim"[^>]*\/>/, '');
}

async function png(markup, size, { opaque = false } = {}) {
	const sized = markup.replace('<svg ', `<svg width="${size}" height="${size}" `);
	const { data, info } = await sharp(Buffer.from(sized), { density: 72 })
		.ensureAlpha()
		.raw()
		.toBuffer({ resolveWithObject: true });
	if (info.width !== size || info.height !== size) throw new Error(`rendered ${info.width}x${info.height}, wanted ${size}`);
	const g = GAMMA[size];
	if (g) for (let i = 3; i < data.length; i += 4) data[i] = Math.round(255 * (data[i] / 255) ** g);
	const img = sharp(data, { raw: info });
	return (opaque ? img.removeAlpha() : img).png({ compressionLevel: 9 }).toBuffer();
}

// ICO with PNG frames: a 6-byte header, a 16-byte entry per frame, then the PNGs.
function ico(frames) {
	const head = Buffer.alloc(6 + 16 * frames.length);
	head.writeUInt16LE(1, 2);
	head.writeUInt16LE(frames.length, 4);
	let offset = head.length;
	frames.forEach(({ size, data }, i) => {
		const e = 6 + 16 * i;
		head.writeUInt8(size % 256, e);
		head.writeUInt8(size % 256, e + 1);
		head.writeUInt16LE(1, e + 4);
		head.writeUInt16LE(32, e + 6);
		head.writeUInt32LE(data.length, e + 8);
		head.writeUInt32LE(offset, e + 12);
		offset += data.length;
	});
	return Buffer.concat([head, ...frames.map((f) => f.data)]);
}

function out(path, data) {
	mkdirSync(dirname(root + path), { recursive: true });
	writeFileSync(root + path, data);
	console.log(`${path.padEnd(44)} ${String(data.length).padStart(6)} bytes`);
}

// Renders one variant into a map of file name -> bytes. Touch tiles draw the hexagon without
// its rim, since the tile takes the rim's place; the hexagon fills 74% of the tile's height,
// inside the maskable safe circle (r = 40%).
async function renderSet(colors) {
	const s = colors.rim ? 1 : INNER;
	const art = paint(colors);
	const logo = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="${box(w * s, h * s)}">${art}</svg>\n`;
	const icon = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="${box(h * s, h * s)}">${art}</svg>\n`;
	const t = Math.round((h * INNER) / 0.74);
	const opaque = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="${box(t, t)}"><rect x="${n(cx - t / 2)}" y="${n(cy - t / 2)}" width="${t}" height="${t}" fill="${colors.tile}"/>${paint(colors, null)}</svg>`;
	const files = { 'logo.svg': logo, 'favicon.svg': icon };
	const frames = [];
	for (const size of [16, 32, 48]) frames.push({ size, data: await png(icon, size) });
	files['favicon.ico'] = ico(frames);
	for (const [name, size] of [['apple-touch-icon', 180], ['apple-touch-icon-167', 167], ['apple-touch-icon-152', 152], ['icon-192', 192], ['icon-512', 512]])
		files[`${name}.png`] = await png(opaque, size, { opaque: true });
	return files;
}

for (const [name, colors] of Object.entries(VARIANTS))
	for (const [file, data] of Object.entries(await renderSet(colors))) out(`brand/variants/${name}/${file}`, data);
