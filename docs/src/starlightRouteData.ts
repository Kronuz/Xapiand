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

import { defineRouteMiddleware } from '@astrojs/starlight/route-data';
import { readerPreferences } from '@kronuz/starlight-core/lib/readerPreferences.mjs';

// Lead the "On this page" list with the page title instead of Starlight's generic
// "Overview". Mutating route.toc here covers both the desktop right-sidebar TOC and the
// mobile "On this page" dropdown (both read route.toc), which replaces the two component
// overrides this used to require. Mirrors the blogs' src/starlightRouteData.ts.
export const onRequest = defineRouteMiddleware((context) => {
	(context.locals as typeof context.locals & { kzFooter?: unknown }).kzFooter = {
		copyright: `© ${new Date().getFullYear()} Germán Méndez Bravo (Kronuz) and © 2015-2020 Dubalu LLC`,
		copyrightHref: `${import.meta.env.BASE_URL.replace(/\/$/, '')}/copyright/`,
		copyrightSuffix: '. Released under the MIT License.',
	};
	context.locals.starlightRoute.head.push({ tag: 'script', content: readerPreferences });
	const route = context.locals.starlightRoute;
	const topItem = route?.toc?.items?.find((it) => it.slug === '_top');
	if (topItem && route.entry?.data?.title) topItem.text = route.entry.data.title;
});
