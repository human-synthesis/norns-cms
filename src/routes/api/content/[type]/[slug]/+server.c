import { route } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { cms } from '$lib/norns/content/server/public'

// GET /api/content/<type>/<slug> — one live public entry plus rendered HTML.
// Members-only entries answer 404 here; members read them on the site.
export GET := route
	handler: async ({ container, event }) =>
		ct := cms(container).getType event.params.type
		unless ct
			throw error 404, 'Unknown content type'
		path := `${ct.basePath || ''}/${event.params.slug}`
		res := await cms(container).resolve path
		if !res || res.kind !== 'entry' || res.locked
			throw error 404, 'Not found'
		{ rendered, ...entry } := res.entry
		return { data: { ...entry, url: res.url }, html: res.html }
