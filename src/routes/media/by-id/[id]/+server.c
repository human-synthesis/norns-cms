import { route } from '@human-synthesis/norns/server'
import { error, redirect } from '@sveltejs/kit'
import { media } from '$lib/norns/media/server/public'

// Stable link for Markdown twins (`media:12` → /media/by-id/12 → the file).
export GET := route
	handler: async ({ container, event }) =>
		m := await media(container).get Number(event.params.id)
		unless m
			throw error 404, 'Not found'
		throw redirect 301, m.url
