import { route } from '@human-synthesis/norns/server'
import { seo } from '$lib/norns/seo/server/public'

export GET := route
	handler: async ({ container }) =>
		new Response(await seo(container).sitemapIndex(), { headers: { 'content-type': 'application/xml; charset=utf-8', 'cache-control': 'public, max-age=600' } })
