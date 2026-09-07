import { page } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { cms } from '$lib/norns/content/server/public'

export load := page.load
	handler: async ({ container, params, url }) =>
		ct := cms(container).getType params.type
		unless ct
			throw error 404, `Unknown content type "${params.type}"`
		status := url.searchParams.get('status') ?? ''
		q := url.searchParams.get('q') ?? ''
		entries := await cms(container).list ct.id, { status: status || undefined, q: q || undefined, limit: 100 }
		total := await cms(container).count ct.id
		published := await cms(container).count ct.id, 'published'
		{ ct, entries, status, q, total, published }
