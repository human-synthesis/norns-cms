import { route } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { seo } from '$lib/norns/seo/server/public'
import { articles } from '$lib/norns/articles/server/public'

export GET := route
	handler: async ({ container, event }) =>
		term := await articles(container).termBySlug 'category', event.params.slug
		unless term
			throw error 404, 'Not found'
		xml := await seo(container).feed { termId: term.id, title: term.name, path: `/articles/category/${term.slug}/feed.xml` }
		new Response(xml, { headers: { 'content-type': 'application/rss+xml; charset=utf-8', 'cache-control': 'public, max-age=600' } })
