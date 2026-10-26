import { route } from '@human-synthesis/norns/server'
import { seo } from '$lib/norns/seo/server/public'

export GET := route
	handler: async ({ container }) =>
		new Response(await seo(container).feed(), { headers: { 'content-type': 'application/rss+xml; charset=utf-8', 'cache-control': 'public, max-age=600' } })
