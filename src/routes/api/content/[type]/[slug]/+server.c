import { route } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { cms, typeOrRedirect } from '$lib/norns/content/server/public'

// GET /api/content/<type>/<slug>?locale= — one live public entry plus rendered HTML.
// Members-only entries answer 404 here; members read them on the site.
export GET := route
	handler: async ({ container, event }) =>
		ct := typeOrRedirect container, event.params.type, event.url
		locale := event.url.searchParams.get('locale') || cms(container).defaultLanguage()
		path := cms(container).sectionPath(ct, locale) + `/${event.params.slug}`
		res := await cms(container).resolve path, {}, { language: locale, market: null, contentFallback: false, chain: [locale] }
		if !res || res.kind !== 'entry' || res.locked
			throw error 404, 'Not found'
		{ rendered, ...entry } := res.entry
		return { data: { ...entry, url: res.url }, html: res.html }
