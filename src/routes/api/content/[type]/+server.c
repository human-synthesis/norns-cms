import { route } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { cms } from '$lib/norns/content/server/public'

// GET /api/content/<type> — published entries, newest first. Content
// negotiated: `Accept: application/tron` gets TRON, everyone else JSON.
export GET := route
	handler: async ({ container, event }) =>
		ct := cms(container).getType event.params.type
		unless ct
			throw error 404, 'Unknown content type'
		data := await cms(container).list ct.id, { status: 'published', limit: 100 }
		return { data }
