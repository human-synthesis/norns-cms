import { route } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { cms } from '$lib/norns/content/server/public'

// GET /api/content/<type>?limit=&offset= — live public entries, newest
// first (members-only entries are never listed here). Content negotiated:
// `Accept: application/tron` gets TRON, everyone else JSON.
export GET := route
	handler: async ({ container, event }) =>
		ct := cms(container).getType event.params.type
		unless ct
			throw error 404, 'Unknown content type'
		limit := Math.min 100, Math.max(1, Number(event.url.searchParams.get('limit')) || 50)
		offset := Math.max 0, Number(event.url.searchParams.get('offset')) || 0
		{ entries, total } := await cms(container).query ct.id, { limit, offset }
		data := entries.map ({ rendered, ...e }) => ({ ...e, url: cms(container).urlFor(ct, e) })
		return { data, total, limit, offset }
