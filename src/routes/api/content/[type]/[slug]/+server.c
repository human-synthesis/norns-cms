import { route } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { cms } from '$lib/norns/content/server/public'

// GET /api/content/<type>/<slug> — one published entry plus rendered HTML.
export GET := route
	handler: async ({ container, event }) =>
		ct := cms(container).getType event.params.type
		unless ct
			throw error 404, 'Unknown content type'
		path := `${ct.basePath || ''}/${event.params.slug}`
		res := await cms(container).resolve path
		if !res || res.kind !== 'entry'
			throw error 404, 'Not found'
		return { data: res.entry, html: res.html }
