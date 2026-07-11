import { defineRouteMiddleware } from '@astrojs/starlight/route-data';

// Lead the "On this page" list with the page title instead of Starlight's generic
// "Overview". Mutating route.toc here covers both the desktop right-sidebar TOC and the
// mobile "On this page" dropdown (both read route.toc), which replaces the two component
// overrides this used to require. Mirrors the blogs' src/starlightRouteData.ts.
export const onRequest = defineRouteMiddleware((context) => {
	const route = context.locals.starlightRoute;
	const topItem = route?.toc?.items?.find((it) => it.slug === '_top');
	if (topItem && route.entry?.data?.title) topItem.text = route.entry.data.title;
});
