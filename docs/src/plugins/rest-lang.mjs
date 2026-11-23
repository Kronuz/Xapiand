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

// A Shiki language for Xapiand REST requests: the stock `http` grammar with the
// request-line verb list widened to Xapiand's full method set, so non-standard verbs
// (INFO, UPDATE, RESTORE, DUMP, OPEN, CLOSE, COMMIT, ...) highlight like GET/PUT do.
// No verb-swapping -- just the one regex that lists the methods. Use it with a ```rest
// fence.
//
// It embeds only the JSON request body (source.json, which our docs always load) and
// drops the http grammar's curl/xml/graphql body embeds. That keeps `rest` a single,
// self-contained language with no extra deps -- registering the whole http bundle
// instead re-registers Shiki's built-in json/js/ts grammars and silently breaks all
// rendering.
import httpBundle from '@shikijs/langs/http';

// Xapiand's full HTTP method set (src/server/search_views.cc METHODS_OPTIONS), plus the
// WebDAV verbs the stock http grammar already knew.
const VERBS = [
	'get', 'post', 'put', 'delete', 'patch', 'head', 'options', 'connect', 'trace',
	'lock', 'unlock', 'propfind', 'proppatch', 'copy', 'move', 'mkcol', 'mkcalendar', 'acl',
	'purge', 'link', 'unlink',
	'search', 'check', 'close', 'commit', 'count', 'dump', 'flush', 'info', 'open',
	'quit', 'restore', 'update', 'upsert', 'wal',
].join('|');

const bundle = structuredClone(httpBundle.default ?? httpBundle);
const rest = bundle.find((g) => g.name === 'http');
rest.name = 'rest';
rest.repository['request-line'].match =
	`(?i)^(${VERBS})\\s+\\s*(.+?)(?:\\s+(HTTP/\\S+))?$`;

// Drop the curl (source.shell), xml (text.xml) and graphql (source.graphql) body
// embeds; keep everything else (request line, headers, comments, metadata, and the
// source.json body). Now `rest` needs nothing beyond source.json.
rest.patterns = rest.patterns.filter(
	(p) => !/source\.shell|text\.xml|source\.graphql/.test(JSON.stringify(p)),
);
// Shiki resolves a language's deps from `embeddedLangs`, not the patterns. Now that only
// the JSON body embed remains, declare just that -- json is always loaded for our docs,
// so `rest` registers cleanly without us re-registering (and clobbering) any built-in.
rest.embeddedLangs = ['json'];

// `rest` needs source.json at registration; provide the bundle's json alongside it.
// (Providing the WHOLE http bundle -- java/js/ts/... too -- is what silently broke
// rendering; json on its own is fine.)
const json = bundle.find((g) => g.name === 'json');
export const restLangs = [rest, json];
