import { route } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { seo } from '$lib/norns/seo/server/public'

export GET := route
	handler: async ({ container }) =>
		body := await seo(container).llmsFull()
		unless body
			throw error 404, 'Not published'
		new Response(body, { headers: { 'content-type': 'text/plain; charset=utf-8', 'cache-control': 'public, max-age=600' } })
