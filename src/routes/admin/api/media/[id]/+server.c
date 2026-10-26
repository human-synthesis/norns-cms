import { route } from '@human-synthesis/norns/server'
import { json, error } from '@sveltejs/kit'
import { media } from '$lib/norns/media/server/public'
import { requireCap } from '$lib/norns/auth/server/public'

export GET := route
	handler: async ({ container, event, user }) =>
		requireCap user, 'media.view'
		m := await media(container).get Number(event.params.id)
		unless m
			throw error 404, 'Not found'
		json m
