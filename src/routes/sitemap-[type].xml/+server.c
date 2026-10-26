import { route } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { seo } from '$lib/norns/seo/server/public'

export GET := route
	handler: async ({ container, event }) =>
		xml := await seo(container).sitemapFor event.params.type
		unless xml
			throw error 404, 'No such sitemap'
		new Response(xml, { headers: { 'content-type': 'application/xml; charset=utf-8', 'cache-control': 'public, max-age=600' } })
