import { page } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { cms } from '$lib/norns/content/server/public'

export load := page.load
	handler: ({ container, params, url }) =>
		ct := cms(container).getType params.type
		unless ct
			throw error 404, `Unknown content type "${params.type}"`
		status := url.searchParams.get('status') ?? ''
		q := url.searchParams.get('q') ?? ''
		entries := cms(container).list ct.id, { status: status || undefined, q: q || undefined, limit: 100 }
		{
			ct,
			entries,
			status,
			q,
			total: cms(container).count(ct.id),
			published: cms(container).count(ct.id, 'published'),
		}
